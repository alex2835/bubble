#include "bubble/editing/editor_lua.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "lua_values.hpp"

namespace bubble
{
EditorLua::EditorLua( Workspace& workspace, const Operators& operators, LuaState::PrintHandler print )
    : mLua( std::move( print ) ),
      mWorkspace( workspace ),
      mOperators( operators )
{
    Bind();
    mLua.Seal();
    // A run's own globals read through to the editor's.
    mRunEnvironment = mLua.NewTable();
    mRunEnvironment["__index"] = mLua.Globals();
    mRunEnvironment.Freeze();
}

expected<void, string> EditorLua::TryRun( string_view source, string_view name )
{
    auto bytecodeMaybe = TryCompileScript( source );
    if ( not bytecodeMaybe )
        // Luau's message starts with the line: ":3: Expected ...".
        return unexpected( string( name ) + bytecodeMaybe.error() );
    LuaTable environment = mLua.NewTable();
    environment.SetMetatable( mRunEnvironment );
    auto chunkMaybe = mLua.TryLoad( name, *bytecodeMaybe, environment );
    if ( not chunkMaybe )
        return unexpected( std::move( chunkMaybe.error() ) );
    if ( auto ranMaybe = mLua.TryCall( *chunkMaybe ); not ranMaybe )
        return unexpected( std::move( ranMaybe.error().mMessage ) );
    return {};
}

void EditorLua::Bind()
{
    LuaTable editor = mLua.NewTable();
    BindOperators( editor );
    BindReads( editor );
    editor.Freeze();
    mLua.Globals()["editor"] = editor;
}

void EditorLua::BindOperators( LuaTable& editor )
{
    editor["run"] = [this]( string id, LuaValue args ) {
        const Value::Record record = ArgsFor( id, args );
        if ( auto ranMaybe = mOperators.TryRun( mWorkspace, id, record ); not ranMaybe )
            throw LuaError( ranMaybe.error() );
    };
    editor["poll"] = [this]( string id, LuaValue args ) -> tuple<bool, LuaValue> {
        const Value::Record record = ArgsFor( id, args );
        if ( auto polledMaybe = mOperators.TryPoll( mWorkspace, id, record ); not polledMaybe )
            return { false, mLua.Value( polledMaybe.error() ) };
        return { true, LuaValue() };
    };

    // editor.ops.entity.rename{ ... }: a table of groups, each a table of
    // operators, made from the operators there are now.
    LuaTable ops = mLua.NewTable();
    string groups;
    for ( const Operator& op : mOperators.All() )
    {
        const size_t dot = op.mId.find( '.' );
        const string group = op.mId.substr( 0, dot );
        LuaValue groupValue = ops.RawGet( group );
        if ( groupValue.IsNil() )
        {
            groupValue = mLua.NewTable();
            ops.RawSet( group, groupValue );
            groups += ( groups.empty() ? "" : ", " ) + group;
        }
        LuaTable( groupValue ).RawSet( op.mId.substr( dot + 1 ), [this, id = op.mId]( LuaValue args ) {
            const Value::Record record = ArgsFor( id, args );
            if ( auto ranMaybe = mOperators.TryRun( mWorkspace, id, record ); not ranMaybe )
                throw LuaError( ranMaybe.error() );
        } );
    }
    // A name that is not there says what is, rather than "attempt to call a
    // nil value" a line later.
    for ( const auto& [groupKey, groupValue] : ops.Pairs() )
    {
        string group = *groupKey.TryAs<string>();
        string verbs;
        for ( const auto& [verbKey, verbValue] : LuaTable( groupValue ).Pairs() )
            verbs += ( verbs.empty() ? "" : ", " ) + *verbKey.TryAs<string>();
        LuaTable metatable = mLua.NewTable();
        metatable["__index"] = [group, verbs]( LuaValue, string verb ) -> LuaValue {
            throw LuaError( format( "{}.{} is not an operator ({} has: {})", group, verb, group, verbs ) );
        };
        LuaTable groupTable( groupValue );
        groupTable.SetMetatable( metatable );
        groupTable.Freeze();
    }
    LuaTable metatable = mLua.NewTable();
    metatable["__index"] = [groups]( LuaValue, string group ) -> LuaValue {
        throw LuaError( format( "no operators are called {}.* (there are: {})", group, groups ) );
    };
    ops.SetMetatable( metatable );
    ops.Freeze();
    editor["ops"] = ops;

    // The functions done inside are one step of the active document's
    // history, labelled `label`; an error inside takes them all back.
    editor["group"] = [this]( string label, LuaFunction fn ) {
        History& history = Active().GetHistory();
        history.BeginGroup( std::move( label ) );
        if ( auto ranMaybe = fn(); not ranMaybe )
        {
            history.CancelGroup();
            throw LuaError( ranMaybe.error().mMessage );
        }
        history.EndGroup();
    };
}

void EditorLua::BindReads( LuaTable& editor )
{
    // A component's field as a value - "light.brightness", "light.steps[2]" -
    // or the whole component - "light" - as a table of its fields.
    editor["get"] = [this]( string path, string field ) -> LuaValue {
        const Document& document = Active();
        const Scene& scene = document.GetScene();
        const Entity entity = Resolve( document, path );
        const size_t dot = field.find( '.' );
        const string componentName = field.substr( 0, dot );
        const ComponentTypes& componentTypes = document.GetComponentTypes();
        const auto nameMaybe = Name::TryFrom( componentName );
        const auto typeMaybe = nameMaybe ? componentTypes.TryFind( *nameMaybe ) : nullopt;
        if ( not typeMaybe )
            throw LuaError(
                format( "{}: {} is not a component (components: {})", path, componentName, componentTypes.Names() ) );
        MetaAny value = typeMaybe->mGet( scene, entity );
        if ( not value )
            throw LuaError( format( "{} has no {}", path, componentName ) );
        Reflection& reflection = componentTypes.GetReflection();
        if ( dot != string::npos )
        {
            const auto fieldPathMaybe = FieldPath::TryParse( string_view( field ).substr( dot + 1 ) );
            if ( not fieldPathMaybe )
                throw LuaError( format( "{}: {}: {}", path, componentName, fieldPathMaybe.error() ) );
            auto gotMaybe = reflection.TryGetField( static_cast<const MetaAny&>( value ), *fieldPathMaybe );
            if ( not gotMaybe )
                throw LuaError( format( "{}: {}", path, gotMaybe.error() ) );
            value = std::move( *gotMaybe );
        }
        auto readMaybe = reflection.TryToValue( value );
        if ( not readMaybe )
            throw LuaError( format( "{}: {}", path, readMaybe.error() ) );
        return LuaFromValue( mLua, *readMaybe );
    };
    editor["children"] = [this]( string path ) {
        const Document& document = Active();
        LuaTable children = mLua.NewTable();
        for ( const Entity child : ChildrenOf( document.GetScene(), Resolve( document, path ) ) )
            children.Append( PathOf( document.GetScene(), child ).mPath.String() );
        return children;
    };
    editor["components"] = [this]( string path ) {
        const Document& document = Active();
        const Entity entity = Resolve( document, path );
        LuaTable components = mLua.NewTable();
        for ( const ComponentTypes::Type& type : document.GetComponentTypes().All() )
            if ( type.mGet( document.GetScene(), entity ) )
                components.Append( type.mName.ToString() );
        return components;
    };
    editor["exists"] = [this]( string path ) {
        const Document& document = Active();
        const auto textMaybe = Text::TryFrom( path );
        return textMaybe and TryResolve( document.GetScene(), EntityPath{ *textMaybe } ).has_value();
    };
    editor["selection"] = [this]() {
        const Document& document = Active();
        LuaTable selected = mLua.NewTable();
        for ( const Entity entity : document.GetSelection().All() )
            selected.Append( PathOf( document.GetScene(), entity ).mPath.String() );
        return selected;
    };
}

Document& EditorLua::Active() const
{
    const auto activeMaybe = mWorkspace.TryActive();
    if ( not activeMaybe )
        throw LuaError( "no document is open" );
    return *activeMaybe;
}

Entity EditorLua::Resolve( const Document& document, const string& path ) const
{
    auto textMaybe = Text::TryFrom( path );
    if ( not textMaybe )
        throw LuaError( textMaybe.error() );
    auto foundMaybe = TryResolve( document.GetScene(), EntityPath{ std::move( *textMaybe ) } );
    if ( not foundMaybe )
        throw LuaError( foundMaybe.error() );
    return *foundMaybe;
}

Value::Record EditorLua::ArgsFor( string_view id, const LuaValue& args )
{
    if ( args.IsNil() )
        return {};
    if ( not args.Is( LuaKind::Table ) )
        throw LuaError( format( "{}: the arguments are a table, not {}", id, mLua.Describe( args ) ) );
    const auto opMaybe = mOperators.TryFind( id );
    Value::Record record;
    for ( const auto& [key, value] : LuaTable( args ).Pairs() )
    {
        const auto keyMaybe = key.TryAs<string>();
        if ( not keyMaybe )
            throw LuaError( format( "{}: an argument is named by a string, not {}", id, mLua.Describe( key ) ) );
        const auto nameMaybe = Name::TryFrom( *keyMaybe );
        if ( not nameMaybe )
            throw LuaError( format( "{}.{}: {}", id, *keyMaybe, nameMaybe.error() ) );
        // An argument the operator does not take is read for what it is,
        // and the operator says it does not take it.
        ValueType type = ValueType::Of( ValueKind::Any );
        if ( opMaybe )
            if ( const auto declared = ranges::find( opMaybe->mArgs, *nameMaybe, &OperatorArg::mName );
                 declared != opMaybe->mArgs.end() )
                type = declared->mType;
        auto readMaybe = TryValueFromLua( mLua, value, type, format( "{}.{}", id, *keyMaybe ) );
        if ( not readMaybe )
            throw LuaError( readMaybe.error() );
        record.push_back( ValueField{ *nameMaybe, std::move( *readMaybe ) } );
    }
    return record;
}
}
