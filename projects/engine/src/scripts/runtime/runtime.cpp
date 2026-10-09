#include "bubble/core/asset_path.hpp"
#include "bubble/scripts/script_runtime.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

// The runtime's construction, the globals scripts see, and aliases.
//
// Script code can create and destroy instances while it runs, so nothing
// in the runtime keeps a reference to an entry across a call into Luau:
// before the call the handle is looked up, after it the handle is looked
// up again.
namespace bubble
{
ScriptRuntime::CurrentScope::CurrentScope( ScriptRuntime& runtime, ScriptInstanceHandle instanceHandle )
    : mRuntime( runtime ),
      mPreviousHandle( runtime.mCurrentHandle )
{
    mRuntime.mCurrentHandle = instanceHandle;
}

ScriptRuntime::CurrentScope::~CurrentScope()
{
    mRuntime.mCurrentHandle = mPreviousHandle;
}

ScriptRuntime::ScriptRuntime( LuaState& lua, AssetRegistry& assets, vector<string> callbacks )
    : mLua( lua ),
      mAssets( assets ),
      mCallbacks( std::move( callbacks ) )
{
    if ( lua.Sealed() )
        throw logic_error( "ScriptRuntime adds globals: build it before the state is sealed" );
    mPathOf = mLua.NewWeakKeyTable();
    mPropsOf = mLua.NewWeakKeyTable();
    mLocalsOf = mLua.NewWeakKeyTable();
    mLibraryOf = mLua.NewWeakKeyTable();
    mRequiresOf = mLua.NewWeakKeyTable();

    mOpenEnvironment = mLua.NewTable();
    mOpenEnvironment["__index"] = mLua.Globals();
    mOpenEnvironment.Freeze();
    mStrictEnvironment = mLua.NewTable();
    mStrictEnvironment["__index"] = mLua.Globals();
    mStrictEnvironment["__newindex"] = []( const LuaValue&, const LuaValue& key ) {
        throw LuaError( format( "assignment to undeclared global '{}': declare it at the top of the file, "
                                "or keep it in self",
                                key.As<string>().value_or( key.Describe() ) ) );
    };
    mStrictEnvironment.Freeze();

    RegisterGlobals();
    mListenerHandle = mAssets.OnChanged( [this]( const AssetEntryBase& entry ) { Changed( entry ); } );
}

ScriptRuntime::~ScriptRuntime()
{
    mAssets.RemoveListener( mListenerHandle );
}

void ScriptRuntime::RegisterGlobals()
{
    LuaTable globals = mLua.Globals();

    globals["props"] = [this]( const LuaTable& props ) {
        const LuaTable file = mLua.CallerEnvironment();
        if ( mLibraryOf[file].Truthy() )
            throw LuaError( "props belong to entity scripts; a library shares through what it returns" );
        if ( not mPropsOf[file].IsNil() )
            throw LuaError( "props is declared once, at the top of the file" );
        mPropsOf[file] = props;
    };

    globals["locals"] = [this]( const LuaTable& locals ) {
        const LuaTable file = mLua.CallerEnvironment();
        if ( mLibraryOf[file].Truthy() )
            throw LuaError( "locals belong to entity scripts; a library keeps its state in its own locals" );
        if ( not mLocalsOf[file].IsNil() )
            throw LuaError( "locals is declared once, at the top of the file" );
        mLocalsOf[file] = locals;
    };

    // The hints are for the inspector and are read on import; at run time
    // a prop is its default.
    globals["prop"] = []( const LuaValue& value, const LuaValue& hints ) {
        if ( not hints.IsNil() and not hints.Is( LuaKind::Table ) )
            throw LuaError( "prop( default, hints ): the hints are a table" );
        return value;
    };

    globals["require"] = [this]( const string& request ) {
        auto value = Require( mLua.CallerEnvironment(), request );
        if ( not value )
            throw LuaError( value.error() );
        return *value;
    };

    globals["start"] = [this]( const LuaFunction& fn, const LuaRest& args ) { Start( Current( "start" ), fn, args ); };

    globals["wait"] = [this]( opt<f64> seconds ) {
        if ( not mLua.Yieldable() )
            throw LuaError( "wait works inside start( fn ), not in a callback" );
        return LuaYield{ mLua.Value( seconds.value_or( 0.0 ) ) };
    };

    globals["wait_until"] = [this]( const LuaFunction& condition ) {
        if ( not mLua.Yieldable() )
            throw LuaError( "wait_until works inside start( fn ), not in a callback" );
        return LuaYield{ condition };
    };

    globals["on"] = [this]( const string& event, const LuaFunction& fn ) {
        const ScriptInstanceHandle instanceHandle = Current( "on" );
        mEvents[event].push_back( Subscriber{ instanceHandle, fn } );
        mInstances.Get( instanceHandle )->mEvents.insert( event );
    };

    globals["emit"] = [this]( const string& event, const LuaRest& args ) { EmitWith( event, args ); };
}

ScriptInstanceHandle ScriptRuntime::Current( const char* what ) const
{
    if ( not mInstances.Alive( mCurrentHandle ) )
        throw LuaError( format( "{} works in an entity's callbacks, not at the top of a file", what ) );
    return mCurrentHandle;
}

expected<void, string> ScriptRuntime::SetAlias( string_view name, string_view directory )
{
    const bool named = not name.empty() and ranges::all_of( name, []( char c ) {
        return ( c >= 'a' and c <= 'z' ) or ( c >= '0' and c <= '9' ) or c == '_' or c == '-';
    } );
    if ( not named )
        return unexpected( format( "alias '{}': lowercase letters, digits, _ and - only", name ) );
    string path;
    if ( not directory.empty() )
    {
        auto valid = AssetPath::From( directory );
        if ( not valid )
            return unexpected( format( "alias @{}: {}", name, valid.error() ) );
        path = valid->String();
    }
    mAliases[string( name )] = std::move( path );
    return {};
}
}
