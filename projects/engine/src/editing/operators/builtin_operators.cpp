// The operators the engine ships. Each one is what a menu item or a hotkey
// used to do inline in the editor, with the selection read from the context
// and everything else from `args`.
#include "engine/pch/pch.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/editing/commands/tree_commands.hpp"
#include "engine/editing/commands/component_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/project/prefab.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/set.hpp"
#include <nlohmann/json.hpp>
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/tag_component.hpp"

namespace bubble
{
namespace
{
Scene& SceneOf( const OperatorContext& ctx ) { return ctx.mLevel.mScene; }

// An entity named in args by id, or the fallback when the key is absent.
Entity EntityArg( const OperatorContext& ctx, const json& args, const char* key, Entity fallback )
{
    if ( not args.contains( key ) )
        return fallback;
    // A path, from the root: "props/chair".
    if ( args.at( key ).is_string() )
    {
        const string path = args.at( key ).get<string>();
        const Entity found = FindByPath( SceneOf( ctx ), SceneOf( ctx ).Root(), path );
        if ( found == Entity::Null )
            throw std::runtime_error( std::format( "{}: nothing at '{}' - {}", key, path,
                                                   WhyPathFails( SceneOf( ctx ), SceneOf( ctx ).Root(), path ) ) );
        return found;
    }
    const auto id = args.at( key ).get<u64>();
    const Entity entity = Entity::FromId( id );
    if ( not SceneOf( ctx ).HasEntity( entity ) )
        throw std::runtime_error( std::format( "{}: no entity {}", key, id ) );
    return entity;
}

// The single selected entity, or Entity::Null.
Entity SelectedOne( const OperatorContext& ctx )
{
    return ctx.mSelection.IsSingleSelection() ? ctx.mSelection.GetSingleEntity() : Entity::Null;
}

// An entity named in args, or the single selected one - required.
Entity RequiredEntity( const OperatorContext& ctx, const json& args, const char* key )
{
    const Entity entity = EntityArg( ctx, args, key, SelectedOne( ctx ) );
    if ( entity == Entity::Null )
        throw std::runtime_error( std::format( "{}: not given and the selection is not one entity", key ) );
    return entity;
}

ComponentTypeId ComponentArg( const json& args )
{
    // GetID throws on an unknown name, naming it.
    return ComponentManager::GetID( args.at( "component" ).get<string>() );
}

// Where a paste or a create lands when no parent is named: inside the
// selected folder, next to the selected entity, or under the root.
Entity TargetParent( const OperatorContext& ctx )
{
    const Scene& scene = SceneOf( ctx );
    const Entity selected = SelectedOne( ctx );
    if ( selected == Entity::Null )
        return scene.Root();
    if ( selected == scene.Root() or scene.HasComponent<FolderComponent>( selected ) )
        return selected;
    const Entity parent = ParentOf( scene, selected );
    return parent != Entity::Null ? parent : scene.Root();
}

bool HasSelection( const OperatorContext& ctx, const json& ) { return not ctx.mSelection.IsEmpty(); }
bool HasOneSelected( const OperatorContext& ctx, const json& )
{
    return ctx.mSelection.IsSingleSelection() and ctx.mSelection.GetSingleEntity() != SceneOf( ctx ).Root();
}
}

void OperatorRegistry::RegisterBuiltins()
{
    auto& registry = Instance();
    if ( registry.Find( "history.undo" ) )
        return;

    /// History
    // Either one may take the selected entity out of the level.
    registry.Register( { "history.undo", "Undo",
        []( const OperatorContext& ctx, const json& ) { return ctx.mHistory.CanUndo(); },
        []( OperatorContext& ctx, const json& )
        {
            ctx.mHistory.Undo();
            ctx.mSelection.Prune( SceneOf( ctx ) );
        } } );
    registry.Register( { "history.redo", "Redo",
        []( const OperatorContext& ctx, const json& ) { return ctx.mHistory.CanRedo(); },
        []( OperatorContext& ctx, const json& )
        {
            ctx.mHistory.Redo();
            ctx.mSelection.Prune( SceneOf( ctx ) );
        } } );

    /// Scene tree
    // args: type (EntityKind name: Folder, ModelObject, PhysicsObject,
    // GameObject, Script, Light, Camera, Audio), parent (entity id, default:
    // by selection), spawn_at (vec3, default: origin). Selects what it made.
    registry.Register( { "scene.create_node", "Create", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const string typeName = args.at( "type" ).get<string>();
            const auto kind = magic_enum::enum_cast<EntityKind>( typeName );
            if ( not kind )
                throw std::runtime_error( std::format( "type: cannot create a '{}'", typeName ) );

            const Entity parent = EntityArg( ctx, args, "parent", TargetParent( ctx ) );
            const Transform spawnAt( args.value( "spawn_at", vec3( 0 ) ) );

            auto command = CreateScope<CreateEntityCommand>( ctx.mProject, SceneOf( ctx ), parent, *kind, spawnAt );
            auto* raw = command.get();
            ctx.mHistory.Execute( std::move( command ) );
            ctx.mSelection.Select( raw->Created(), SceneOf( ctx ) );
        } } );

    // Deletes the selection, with what is under it. args: none.
    registry.Register( { "scene.delete", "Delete", HasSelection,
        []( OperatorContext& ctx, const json& )
        {
            const vector<Entity> entities( ctx.mSelection.GetEntities().begin(), ctx.mSelection.GetEntities().end() );
            ctx.mSelection.Clear();
            auto command = CreateScope<DeleteEntitiesCommand>( SceneOf( ctx ), entities );
            ctx.mHistory.Execute( std::move( command ) );
        } } );

    // Clipboard. Cut and copy only note the entity; paste is the edit.
    registry.Register( { "scene.cut", "Cut", HasOneSelected,
        []( OperatorContext& ctx, const json& )
        {
            ctx.mClipboard.Cut( ctx.mSelection.GetSingleEntity() );
            ctx.mSelection.Clear();
        } } );
    registry.Register( { "scene.copy", "Copy", HasOneSelected,
        []( OperatorContext& ctx, const json& )
        {
            ctx.mClipboard.Copy( ctx.mSelection.GetSingleEntity() );
        } } );
    // args: parent (entity id, default: by selection). Either way the pasted
    // entity stays where it was in the world.
    registry.Register( { "scene.paste", "Paste",
        []( const OperatorContext& ctx, const json& ) { return not ctx.mClipboard.IsEmpty(); },
        []( OperatorContext& ctx, const json& args )
        {
            Scene& scene = SceneOf( ctx );
            const Entity source = ctx.mClipboard.GetEntity();
            if ( not scene.HasEntity( source ) )
                throw std::runtime_error( "paste: what was cut or copied is gone" );
            const Entity parent = EntityArg( ctx, args, "parent", TargetParent( ctx ) );
            if ( ctx.mClipboard.IsCut() )
            {
                if ( parent == source or IsAncestor( scene, source, parent ) )
                    throw std::runtime_error( "paste: cannot move an entity into itself" );
                ctx.mHistory.Execute( CreateScope<MoveEntityCommand>( scene, source, parent ) );
                ctx.mClipboard.Clear();
                ctx.mSelection.Select( source, scene );
            }
            else
            {
                auto command = CreateScope<CopyEntityCommand>( scene, source, parent );
                auto* raw = command.get();
                ctx.mHistory.Execute( std::move( command ) );
                ctx.mSelection.Select( raw->Copy(), scene );
            }
        } } );

    // args: entity (id), parent (entity id, default: the root), index
    // (default: last). Keeps it where it is in the world.
    registry.Register( { "scene.move", "Move", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            Scene& scene = SceneOf( ctx );
            const Entity entity = RequiredEntity( ctx, args, "entity" );
            const Entity parent = EntityArg( ctx, args, "parent", scene.Root() );
            if ( entity == scene.Root() or parent == entity or IsAncestor( scene, entity, parent ) )
                throw std::runtime_error( "move: not into itself, and not the root" );
            ctx.mHistory.Execute( CreateScope<MoveEntityCommand>( scene, entity, parent, args.value( "index", size_t( -1 ) ) ) );
        } } );

    // args: name, entity (id or path, default: the single selected one). A
    // name a sibling has gets a number.
    registry.Register( { "scene.rename", "Rename", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            if ( auto step = MakeRenameCommand( SceneOf( ctx ), RequiredEntity( ctx, args, "entity" ), args.at( "name" ).get<string>() ) )
                ctx.mHistory.Execute( std::move( step ) );
        } } );

    /// Components
    // args: component (name), entity (id, default: the single selected one).
    registry.Register( { "entity.add_component", "Add component", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const Entity entity = RequiredEntity( ctx, args, "entity" );
            const auto componentId = ComponentArg( args );
            if ( SceneOf( ctx ).HasComponent( entity, componentId ) )
                return;
            ctx.mHistory.Execute( CreateScope<AddComponentCommand>( entity, componentId, ctx.mProject, SceneOf( ctx ) ) );
        } } );
    registry.Register( { "entity.remove_component", "Remove component", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const Entity entity = RequiredEntity( ctx, args, "entity" );
            const auto componentId = ComponentArg( args );
            if ( componentId == TagComponent::ID() )
                throw std::runtime_error( "component: the Tag component cannot be removed" );
            if ( not SceneOf( ctx ).HasComponent( entity, componentId ) )
                return;
            ctx.mHistory.Execute( CreateScope<RemoveComponentCommand>( entity, componentId, SceneOf( ctx ) ) );
        } } );

    /// Prefabs
    // args: file (.prefab, relative to the project root), parent (entity id,
    // default: by selection), spawn_at (vec3, default: origin). Selects the
    // instance.
    registry.Register( { "prefab.instantiate", "Instantiate prefab", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const path file = args.at( "file" ).get<string>();
            if ( filesystem::weakly_canonical( ctx.mProject.RootDir() / file ) == filesystem::weakly_canonical( ctx.mLevel.mFile ) )
                throw std::runtime_error( "file: a prefab cannot contain itself" );
            PrefabPlacement placement;
            placement.mWorldPosition = args.value( "spawn_at", vec3( 0 ) );
            const Entity parent = EntityArg( ctx, args, "parent", TargetParent( ctx ) );
            auto command = CreateScope<InstantiatePrefabCommand>( ctx.mProject, SceneOf( ctx ), parent, file, placement );
            auto* raw = command.get();
            ctx.mHistory.Execute( std::move( command ) );
            ctx.mSelection.Select( raw->Root(), SceneOf( ctx ) );
        } } );

    // Writes an entity and what is under it to a prefab file. Not an edit of
    // the level, so no step. args: file (relative, .prefab added), entity
    // (id, default: the single selected one).
    registry.Register( { "prefab.save", "Save as prefab", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            path file = args.at( "file" ).get<string>();
            if ( file.extension() != PREFAB_FILE_EXT )
                file += PREFAB_FILE_EXT;
            const Entity entity = RequiredEntity( ctx, args, "entity" );
            if ( entity == SceneOf( ctx ).Root() )
                throw std::runtime_error( "entity: pick one other than the root" );
            SavePrefab( SceneOf( ctx ), entity, ctx.mProject.RootDir() / file, ctx.mProject );
        } } );

    // Makes every instance of a prefab in the open document again from the
    // file, as one step. args: file (relative; default: every prefab used).
    registry.Register( { "prefab.update_instances", "Update prefab instances", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            Scene& scene = SceneOf( ctx );
            vector<string> files;
            if ( args.contains( "file" ) )
                files.push_back( path( args.at( "file" ).get<string>() ).generic_string() );
            else
            {
                set<string> used;
                for ( const Entity instance : FindPrefabInstances( scene, "" ) )
                    used.insert( scene.GetComponent<PrefabInstanceComponent>( instance ).mPrefab );
                files.assign( used.begin(), used.end() );
            }

            auto step = CreateScope<CompositeCommand>( "Update prefab instances" );
            for ( const string& file : files )
                for ( const Entity instance : FindPrefabInstances( scene, file ) )
                    step->Add( MakeRefreshPrefabInstance( ctx.mProject, scene, instance ) );
            if ( step->Empty() )
                return;
            ctx.mSelection.Clear();
            ctx.mHistory.Execute( std::move( step ) );
        } } );
}

}
