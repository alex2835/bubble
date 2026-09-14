// The operators the engine ships. Each one is what a menu item or a hotkey
// used to do inline in the editor, with the selection read from the context
// and everything else from `args`.
#include "engine/pch/pch.hpp"
#include "engine/editing/operator.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/editing/scene_commands.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/serialization/types_serialization.hpp"
#include <nlohmann/json.hpp>

namespace bubble
{
namespace
{
Scene& SceneOf( const OperatorContext& ctx ) { return ctx.mProject.mLevel.mScene; }
const Ref<ProjectTreeNode>& RootOf( const OperatorContext& ctx ) { return ctx.mProject.mLevel.mTreeRoot; }

// A node named in args by id, or the fallback when the key is absent.
Ref<ProjectTreeNode> NodeArg( const OperatorContext& ctx, const json& args, const char* key, Ref<ProjectTreeNode> fallback )
{
    if ( not args.contains( key ) )
        return fallback;
    auto node = FindNodeById( args.at( key ).get<u64>(), RootOf( ctx ) );
    if ( not node )
        throw std::runtime_error( std::format( "{}: no node with id {}", key, args.at( key ).get<u64>() ) );
    return node;
}

// An entity named in args by id, or the single selected one.
Entity EntityArg( const OperatorContext& ctx, const json& args, const char* key )
{
    if ( args.contains( key ) )
    {
        const auto id = args.at( key ).get<u64>();
        const Entity entity = SceneOf( ctx ).GetEntityById( id );
        if ( not SceneOf( ctx ).HasEntity( entity ) )
            throw std::runtime_error( std::format( "{}: no entity {}", key, id ) );
        return entity;
    }
    if ( not ctx.mSelection.IsSingleSelection() )
        throw std::runtime_error( std::format( "{}: not given and the selection is not one entity", key ) );
    return ctx.mSelection.GetSingleEntity();
}

ComponentTypeId ComponentArg( const json& args )
{
    // GetID throws on an unknown name, naming it.
    return ComponentManager::GetID( args.at( "component" ).get<string>() );
}

// Where a paste or a create lands when no parent is named: inside the
// selected folder, next to the selected entity, or at the root.
Ref<ProjectTreeNode> TargetParent( const OperatorContext& ctx )
{
    const auto& selected = ctx.mSelection.GetTreeNode();
    if ( not selected )
        return RootOf( ctx );
    if ( not selected->IsEntity() )
        return selected;
    auto parent = selected->mParent.lock();
    return parent ? parent : RootOf( ctx );
}

// The nodes the selection stands for: the tree node if one was picked in
// the tree, otherwise the node of each entity picked in the viewport.
vector<Ref<ProjectTreeNode>> SelectedNodes( const OperatorContext& ctx )
{
    vector<Ref<ProjectTreeNode>> nodes;
    if ( ctx.mSelection.GetTreeNode() )
    {
        nodes.push_back( ctx.mSelection.GetTreeNode() );
        return nodes;
    }
    for ( const auto entity : ctx.mSelection.GetEntities() )
        if ( auto node = FindNodeByEntity( entity, RootOf( ctx ) ) )
            nodes.push_back( node );
    return nodes;
}

bool HasSelection( const OperatorContext& ctx, const json& ) { return not ctx.mSelection.IsEmpty(); }
bool HasTreeSelection( const OperatorContext& ctx, const json& ) { return ctx.mSelection.GetTreeNode() != nullptr; }
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
            ctx.mSelection.Prune( SceneOf( ctx ), RootOf( ctx ) );
        } } );
    registry.Register( { "history.redo", "Redo",
        []( const OperatorContext& ctx, const json& ) { return ctx.mHistory.CanRedo(); },
        []( OperatorContext& ctx, const json& )
        {
            ctx.mHistory.Redo();
            ctx.mSelection.Prune( SceneOf( ctx ), RootOf( ctx ) );
        } } );

    /// Scene tree
    // args: type (ProjectTreeNodeType name), parent (node id, default: by
    // selection), spawn_at (vec3, default: origin). Selects what it made.
    registry.Register( { "scene.create_node", "Create node", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const string typeName = args.at( "type" ).get<string>();
            const auto type = magic_enum::enum_cast<ProjectTreeNodeType>( typeName );
            if ( not type or *type == ProjectTreeNodeType::Root )
                throw std::runtime_error( std::format( "type: cannot create a '{}'", typeName ) );

            const auto parent = NodeArg( ctx, args, "parent", TargetParent( ctx ) );
            const Transform spawnAt( args.value( "spawn_at", vec3( 0 ) ) );

            auto command = CreateScope<CreateNodeCommand>( parent, *type, ctx.mProject, spawnAt );
            auto* raw = command.get();
            ctx.mHistory.Execute( std::move( command ) );
            ctx.mSelection.SelectTreeNode( raw->GetCreatedNode(), SceneOf( ctx ) );
        } } );

    // Deletes the selection. args: none.
    registry.Register( { "scene.delete", "Delete", HasSelection,
        []( OperatorContext& ctx, const json& )
        {
            const auto nodes = SelectedNodes( ctx );
            ctx.mSelection.Clear();
            if ( nodes.empty() )
                return;
            if ( nodes.size() == 1 )
                ctx.mHistory.Execute( CreateScope<DeleteNodeCommand>( nodes[0], SceneOf( ctx ) ) );
            else
                ctx.mHistory.Execute( CreateScope<DeleteMultipleNodesCommand>( nodes, SceneOf( ctx ) ) );
        } } );

    // Clipboard. Cut and copy only note the node; paste is the edit.
    registry.Register( { "scene.cut", "Cut", HasTreeSelection,
        []( OperatorContext& ctx, const json& )
        {
            ctx.mClipboard.Cut( ctx.mSelection.GetTreeNode() );
            ctx.mSelection.Clear();
        } } );
    registry.Register( { "scene.copy", "Copy", HasTreeSelection,
        []( OperatorContext& ctx, const json& )
        {
            ctx.mClipboard.Copy( ctx.mSelection.GetTreeNode() );
        } } );
    // args: parent (node id, default: by selection).
    registry.Register( { "scene.paste", "Paste",
        []( const OperatorContext& ctx, const json& ) { return not ctx.mClipboard.IsEmpty(); },
        []( OperatorContext& ctx, const json& args )
        {
            const auto parent = NodeArg( ctx, args, "parent", TargetParent( ctx ) );
            if ( ctx.mClipboard.IsCut() )
            {
                ctx.mHistory.Execute( CreateScope<MoveNodeCommand>( ctx.mClipboard.GetNode(), parent ) );
                ctx.mClipboard.Clear();
            }
            else
                ctx.mHistory.Execute( CreateScope<CopyNodeCommand>( ctx.mClipboard.GetNode(), parent, SceneOf( ctx ) ) );
        } } );

    /// Components
    // args: component (name), entity (id, default: the single selected one).
    registry.Register( { "entity.add_component", "Add component", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const Entity entity = EntityArg( ctx, args, "entity" );
            const auto componentId = ComponentArg( args );
            if ( SceneOf( ctx ).EntityComponentTypeIds( entity ).contains( componentId ) )
                return;
            ctx.mHistory.Execute( CreateScope<AddComponentCommand>( entity, componentId, ctx.mProject ) );
        } } );
    registry.Register( { "entity.remove_component", "Remove component", nullptr,
        []( OperatorContext& ctx, const json& args )
        {
            const Entity entity = EntityArg( ctx, args, "entity" );
            const auto componentId = ComponentArg( args );
            if ( componentId == TagComponent::ID() )
                throw std::runtime_error( "component: the Tag component cannot be removed" );
            if ( not SceneOf( ctx ).EntityComponentTypeIds( entity ).contains( componentId ) )
                return;
            ctx.mHistory.Execute( CreateScope<RemoveComponentCommand>( entity, componentId, SceneOf( ctx ) ) );
        } } );
}

}
