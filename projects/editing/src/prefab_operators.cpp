#include "bubble/editing/edits.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "operator_helpers.hpp"
#include "prefabs.hpp"

namespace bubble
{
namespace
{
using Result = expected<void, string>;

expected<AssetPath, string> TryPrefabPathArg( const OperatorArgs& args, Name name, string_view where )
{
    auto pathMaybe = AssetPath::TryFrom( args.TryGet<Text>( name )->View() );
    if ( not pathMaybe )
        return unexpected( format( "{}: {}", where, pathMaybe.error() ) );
    if ( not IsPrefabPath( *pathMaybe ) )
        return unexpected( format( "{}: a prefab is a .prefab, not {}", where, pathMaybe->View() ) );
    return *pathMaybe;
}

// The instance a node is of: its root, its node's id and the prefab.
struct InstanceNode
{
    Entity mRoot;
    u32 mNode = 0;
    AssetPath mPath;
    Ref<const Prefab> mPrefab;
};

expected<InstanceNode, string> TryInstanceNode( Workspace& workspace, const Document& document, Entity entity )
{
    const Scene& scene = document.GetScene();
    const auto linkMaybe = scene.TryGet<PrefabLink>( entity );
    if ( not linkMaybe or not scene.Has<PrefabInstance>( linkMaybe->mInstance ) )
        return unexpected( format( "{} is no node of a prefab's instance", Describe( scene, entity ) ) );
    const AssetPath& path = scene.Get<PrefabInstance>( linkMaybe->mInstance ).mPrefab;
    auto prefabMaybe = workspace.TryPrefab( path );
    if ( not prefabMaybe )
        return unexpected( format( "{}: {}", path.View(), prefabMaybe.error() ) );
    if ( not( *prefabMaybe )->TryFind( linkMaybe->mNode ) )
        return unexpected( format( "{} has no node {} any more", path.View(), linkMaybe->mNode ) );
    return InstanceNode{
        .mRoot = linkMaybe->mInstance, .mNode = linkMaybe->mNode, .mPath = path, .mPrefab = std::move( *prefabMaybe )
    };
}

// "light.brightness" as the component and the path in it; "light" alone is
// the whole component, the path left empty.
struct ComponentField
{
    Name mComponent;
    opt<FieldPath> mPathMaybe;
};

expected<ComponentField, string> TryComponentField( const OperatorArgs& args, const ComponentTypes& componentTypes )
{
    const string text = args.TryGet<Text>( "path"_n )->String();
    const size_t dot = text.find( '.' );
    const string componentName = text.substr( 0, dot );
    const auto nameMaybe = Name::TryFrom( componentName );
    if ( not nameMaybe or not componentTypes.TryFind( *nameMaybe ) )
        return unexpected(
            format( "path: {} is not a component (components: {})", componentName, componentTypes.Names() ) );
    if ( dot == string::npos )
        return ComponentField{ .mComponent = *nameMaybe, .mPathMaybe = nullopt };
    auto pathMaybe = FieldPath::TryParse( string_view( text ).substr( dot + 1 ) );
    if ( not pathMaybe )
        return unexpected( format( "path: {}", pathMaybe.error() ) );
    return ComponentField{ .mComponent = *nameMaybe, .mPathMaybe = std::move( *pathMaybe ) };
}

OptRef<const Value> TryBaseValue( const Prefab::Node& node, Name component )
{
    const auto found = ranges::find( node.mComponents, component, &pair<Name, Value>::first );
    if ( found == node.mComponents.end() )
        return nullopt;
    return found->second;
}

// The value at `path` in a component made from `whole`.
expected<Value, string> TryValueAt( const Value& whole,
                                    const ComponentTypes::Type& type,
                                    const FieldPath& path,
                                    const ComponentTypes& componentTypes )
{
    Reflection& reflection = componentTypes.GetReflection();
    auto madeMaybe = reflection.TryFromValue( whole, type.mType );
    if ( not madeMaybe )
        return unexpected( std::move( madeMaybe.error() ) );
    auto fieldMaybe = reflection.TryGetField( static_cast<const MetaAny&>( *madeMaybe ), path );
    if ( not fieldMaybe )
        return unexpected( std::move( fieldMaybe.error() ) );
    return reflection.TryToValue( *fieldMaybe );
}

Result RunInstantiate( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    auto pathMaybe = TryPrefabPathArg( args, "prefab"_n, "prefab" );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    auto prefabMaybe = workspace.TryPrefab( *pathMaybe );
    if ( not prefabMaybe )
        return unexpected( format( "prefab: {}: {}", pathMaybe->View(), prefabMaybe.error() ) );
    Entity parent = scene.Root();
    if ( const auto parentPathMaybe = args.TryGet<EntityPath>( "parent"_n ) )
    {
        auto parentMaybe = TryResolveArg( document, *parentPathMaybe, "parent" );
        if ( not parentMaybe )
            return unexpected( std::move( parentMaybe.error() ) );
        parent = *parentMaybe;
    }
    else if ( document.GetSelection().Active() != Entity::Null )
        parent = document.GetSelection().Active();
    const auto indexMaybe = TryIndex( args );
    if ( not indexMaybe )
        return unexpected( indexMaybe.error() );
    auto rootMaybe = TryInstantiatePrefab( document, std::move( *prefabMaybe ), *pathMaybe, parent,
                                           TextOr( args, "name"_n, pathMaybe->Stem() ), *indexMaybe );
    if ( not rootMaybe )
        return unexpected( std::move( rootMaybe.error() ) );
    document.GetSelection().Select( *rootMaybe );
    return {};
}

// The entity and what is under it written as a new prefab, and put back in
// its place as an instance of it, under its name: links to it still lead to
// it. Undo brings the plain entities back; the file stays.
Result RunCreate( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const Scene& scene = document.GetScene();
    if ( document.TryPrefabNodes() )
        return unexpected( "a prefab inside a prefab is not there yet"s );
    auto targetMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not targetMaybe )
        return unexpected( std::move( targetMaybe.error() ) );
    const Entity target = *targetMaybe;
    if ( target == scene.Root() )
        return unexpected( "the root is the scene: a prefab is made of what is under it"s );
    if ( scene.Has<PrefabLink>( target ) )
        return unexpected( format( "{} is of a prefab already: a prefab of an instance is not there yet",
                                   Describe( scene, target ) ) );
    auto pathMaybe = TryPrefabPathArg( args, "path"_n, "path" );
    if ( not pathMaybe )
        return unexpected( std::move( pathMaybe.error() ) );
    if ( workspace.GetFiles().TryRead( *pathMaybe ) or workspace.TryFind( *pathMaybe ) )
        return unexpected( format( "path: {} is there already: give another", pathMaybe->View() ) );

    hmap<Entity, u32> ids;
    auto prefabMaybe = Prefab::TryFromScene( scene, target, workspace.GetComponentTypes(), ids, 1 );
    if ( not prefabMaybe )
        return unexpected( std::move( prefabMaybe.error() ) );
    const Ref<const Prefab> prefab = CreateRef<const Prefab>( std::move( *prefabMaybe ) );
    if ( auto writtenMaybe = TryWritePrefab( workspace, *pathMaybe, prefab, nullopt ); not writtenMaybe )
        return writtenMaybe;

    const Text name = NameOf( scene, target );
    const Entity parent = ParentOf( scene, target );
    const size_t index = IndexInParent( scene, target );
    if ( auto destroyedMaybe = TryDestroyEntity( document, target ); not destroyedMaybe )
        return destroyedMaybe;
    auto rootMaybe = TryInstantiatePrefab( document, prefab, *pathMaybe, parent, name, index );
    if ( not rootMaybe )
        return unexpected( std::move( rootMaybe.error() ) );
    document.GetSelection().Select( *rootMaybe );
    return {};
}

Result RunUnpack( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    auto targetMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not targetMaybe )
        return unexpected( std::move( targetMaybe.error() ) );
    return TryUnpackPrefab( document, *targetMaybe );
}

// A component of a node made what the prefab has: a field of it, or all of
// it - added back when the instance took it off, taken off when the prefab
// has none.
Result RevertComponent( Document& document,
                        Entity entity,
                        const ComponentTypes::Type& type,
                        OptRef<const Value> baseMaybe,
                        const opt<FieldPath>& pathMaybe )
{
    const ComponentTypes& componentTypes = document.GetComponentTypes();
    const bool has = static_cast<bool>( type.mGet( document.GetScene(), entity ) );
    if ( not baseMaybe )
    {
        if ( pathMaybe )
            return unexpected( format( "the prefab has no {} there", type.mName.ToString() ) );
        return has ? TryRemoveComponent( document, entity, type.mName ) : Result();
    }
    if ( not has )
        if ( auto addedMaybe = TryAddComponent( document, entity, type.mName ); not addedMaybe )
            return addedMaybe;
    if ( pathMaybe )
    {
        auto valueMaybe = TryValueAt( *baseMaybe, type, *pathMaybe, componentTypes );
        if ( not valueMaybe )
            return unexpected( std::move( valueMaybe.error() ) );
        return TrySetField( document, entity, type.mName, *pathMaybe, MetaAny( *valueMaybe ) );
    }
    for ( const ValueField& field : *baseMaybe->TryAs<Value::Record>() )
        if ( auto setMaybe =
                 TrySetField( document, entity, type.mName, FieldPath().Then( field.mName ), MetaAny( field.mValue ) );
             not setMaybe )
            return setMaybe;
    return {};
}

// A node, or a component or a field of it, made what its prefab has again.
Result RunRevert( Workspace& workspace, const OperatorArgs& args )
{
    Document& document = Active( workspace );
    const ComponentTypes& componentTypes = document.GetComponentTypes();
    auto targetMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not targetMaybe )
        return unexpected( std::move( targetMaybe.error() ) );
    auto instanceMaybe = TryInstanceNode( workspace, document, *targetMaybe );
    if ( not instanceMaybe )
        return unexpected( std::move( instanceMaybe.error() ) );
    const Prefab::Node& node = *instanceMaybe->mPrefab->TryFind( instanceMaybe->mNode );
    const EntityPath rootPath = PathOf( document.GetScene(), instanceMaybe->mRoot );
    // The prefab's value as this instance has it: its links lead within it.
    const auto base = [&]( Name component ) -> opt<Value> {
        const auto valueMaybe = TryBaseValue( node, component );
        return valueMaybe ? opt<Value>( ToInstance( *valueMaybe, rootPath ) ) : nullopt;
    };
    if ( args.Has( "path"_n ) )
    {
        auto fieldMaybe = TryComponentField( args, componentTypes );
        if ( not fieldMaybe )
            return unexpected( std::move( fieldMaybe.error() ) );
        const opt<Value> baseValue = base( fieldMaybe->mComponent );
        return RevertComponent( document, *targetMaybe, *componentTypes.TryFind( fieldMaybe->mComponent ),
                                baseValue ? OptRef<const Value>( *baseValue ) : nullopt, fieldMaybe->mPathMaybe );
    }
    for ( const ComponentTypes::Type& type : componentTypes.All() )
    {
        const opt<Value> baseValue = base( type.mName );
        if ( auto revertedMaybe = RevertComponent( document, *targetMaybe, type,
                                                   baseValue ? OptRef<const Value>( *baseValue ) : nullopt, nullopt );
             not revertedMaybe )
            return revertedMaybe;
    }
    return {};
}

// What a node of an instance has - a field, a component, or all of its
// components - written into its prefab, which every instance open follows.
// The prefab's file changes, not a document: no step to undo.
Result RunApply( Workspace& workspace, const OperatorArgs& args )
{
    const auto documentMaybe = workspace.TryActive();
    if ( not documentMaybe )
        return NoDocument();
    const Document& document = *documentMaybe;
    const Scene& scene = document.GetScene();
    const ComponentTypes& componentTypes = workspace.GetComponentTypes();
    Reflection& reflection = componentTypes.GetReflection();
    auto targetMaybe = TryTarget( document, args, "entity"_n, "entity" );
    if ( not targetMaybe )
        return unexpected( std::move( targetMaybe.error() ) );
    const Entity entity = *targetMaybe;
    auto instanceMaybe = TryInstanceNode( workspace, document, entity );
    if ( not instanceMaybe )
        return unexpected( std::move( instanceMaybe.error() ) );
    if ( const auto openMaybe = workspace.TryFind( instanceMaybe->mPath ); openMaybe and openMaybe->IsDirty() )
        return unexpected(
            format( "{} is open with changes not saved: save it or close it first", instanceMaybe->mPath.View() ) );
    const Prefab::Node& node = *instanceMaybe->mPrefab->TryFind( instanceMaybe->mNode );
    const EntityPath rootPath = PathOf( scene, instanceMaybe->mRoot );

    // The instance's component as the prefab keeps one: links from its root.
    const auto instanceValue = [&]( const ComponentTypes::Type& type ) -> expected<opt<Value>, string> {
        const MetaAny component = type.mGet( scene, entity );
        if ( not component )
            return opt<Value>();
        auto valueMaybe = reflection.TryToValue( component );
        if ( not valueMaybe )
            return unexpected( std::move( valueMaybe.error() ) );
        return opt<Value>( FromInstance( std::move( *valueMaybe ), rootPath ) );
    };

    // Every component, or the one named, as the instance has it now.
    opt<ComponentField> fieldMaybe;
    if ( args.Has( "path"_n ) )
    {
        auto readMaybe = TryComponentField( args, componentTypes );
        if ( not readMaybe )
            return unexpected( std::move( readMaybe.error() ) );
        fieldMaybe = std::move( *readMaybe );
    }
    vector<pair<Name, Value>> components;
    for ( const ComponentTypes::Type& type : componentTypes.All() )
    {
        const auto baseMaybe = TryBaseValue( node, type.mName );
        if ( fieldMaybe and fieldMaybe->mComponent != type.mName )
        {
            if ( baseMaybe )
                components.emplace_back( type.mName, *baseMaybe );
            continue;
        }
        auto valueMaybe = instanceValue( type );
        if ( not valueMaybe )
            return unexpected( std::move( valueMaybe.error() ) );
        if ( fieldMaybe and fieldMaybe->mPathMaybe )
        {
            // One field into the prefab's component, the rest as it was.
            if ( not baseMaybe or not *valueMaybe )
                return unexpected( format( "{} and its prefab do not both have a {}", Describe( scene, entity ),
                                           type.mName.ToString() ) );
            auto fieldValueMaybe = TryValueAt( **valueMaybe, type, *fieldMaybe->mPathMaybe, componentTypes );
            if ( not fieldValueMaybe )
                return unexpected( std::move( fieldValueMaybe.error() ) );
            auto madeMaybe = reflection.TryFromValue( *baseMaybe, type.mType );
            if ( not madeMaybe )
                return unexpected( std::move( madeMaybe.error() ) );
            if ( auto setMaybe =
                     reflection.TrySetField( *madeMaybe, *fieldMaybe->mPathMaybe, MetaAny( *fieldValueMaybe ) );
                 not setMaybe )
                return unexpected( std::move( setMaybe.error() ) );
            auto wholeMaybe = reflection.TryToValue( *madeMaybe );
            if ( not wholeMaybe )
                return unexpected( std::move( wholeMaybe.error() ) );
            components.emplace_back( type.mName, std::move( *wholeMaybe ) );
            continue;
        }
        if ( *valueMaybe )
            components.emplace_back( type.mName, std::move( **valueMaybe ) );
    }

    Prefab updated = *instanceMaybe->mPrefab;
    updated.SetComponents( instanceMaybe->mNode, std::move( components ) );
    return TryWritePrefab( workspace, instanceMaybe->mPath, CreateRef<const Prefab>( std::move( updated ) ), nullopt );
}
}

void AddPrefabOperators( Operators& operators )
{
    operators.Add( Operator{ .mId = "prefab.instantiate",
                             .mLabel = "Instantiate",
                             .mArgs = { Arg( "prefab", ValueKind::Text, cRequired ), Arg( "parent", ValueKind::Entity ),
                                        Arg( "name", ValueKind::Text ), Arg( "index", ValueKind::Integer ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunInstantiate } );
    operators.Add( Operator{ .mId = "prefab.create",
                             .mLabel = "Make Prefab",
                             .mArgs = { Arg( "entity", ValueKind::Entity ), Arg( "path", ValueKind::Text, cRequired ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunCreate } );
    operators.Add( Operator{ .mId = "prefab.unpack",
                             .mLabel = "Unpack",
                             .mArgs = { Arg( "entity", ValueKind::Entity ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunUnpack } );
    operators.Add( Operator{ .mId = "prefab.revert",
                             .mLabel = "Revert to Prefab",
                             .mArgs = { Arg( "entity", ValueKind::Entity ), Arg( "path", ValueKind::Text ) },
                             .mEdits = true,
                             .mTryPoll = {},
                             .mTryRun = RunRevert } );
    operators.Add( Operator{ .mId = "prefab.apply",
                             .mLabel = "Apply to Prefab",
                             .mArgs = { Arg( "entity", ValueKind::Entity ), Arg( "path", ValueKind::Text ) },
                             .mEdits = false,
                             .mTryPoll = {},
                             .mTryRun = RunApply } );
}
}
