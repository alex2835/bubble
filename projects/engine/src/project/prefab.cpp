#include "engine/pch/pch.hpp"
#include "engine/project/prefab.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/prefab_instance_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/types/set.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
void RemapTable( Table table, const map<Entity, Entity>& copied, set<const void*>& visited )
{
    if ( not visited.insert( table.pointer() ).second )
        return;
    vector<std::pair<sol::object, sol::object>> changes;
    vector<Table> nested;
    for ( const auto& [key, value] : table )
    {
        if ( value.is<Entity>() )
        {
            const auto it = copied.find( value.as<Entity>() );
            changes.emplace_back( key, it != copied.end() ? sol::make_object<Entity>( table.lua_state(), Entity( it->second ) )
                                                          : sol::make_object( table.lua_state(), sol::lua_nil ) );
        }
        else if ( value.get_type() == sol::type::table )
            nested.push_back( value.as<Table>() );
    }
    for ( const auto& [key, value] : changes )
        table.raw_set( key, value );
    for ( const Table& t : nested )
        RemapTable( t, copied, visited );
}

// The entity nodes of a subtree that hang from nothing inside it.
void TopEntities( const Ref<ProjectTreeNode>& node, vector<Entity>& out )
{
    if ( const auto entity = node->TryGetEntity() )
    {
        out.push_back( *entity );
        return;
    }
    for ( const auto& child : node->mChildren )
        TopEntities( child, out );
}

void InsertChild( const Ref<ProjectTreeNode>& parent, const Ref<ProjectTreeNode>& node, size_t index )
{
    node->mParent = parent;
    auto& children = parent->mChildren;
    children.insert( children.begin() + std::min( index, children.size() ), node );
}

Ref<ProjectTreeNode> TreeRootOf( Ref<ProjectTreeNode> node )
{
    while ( auto parent = node->mParent.lock() )
        node = parent;
    return node;
}

// The prefab's root: its single top entity, unless that is itself an
// instance of another prefab - its own link must not be written over.
opt<Ref<ProjectTreeNode>> SingleRoot( const Level& prefab )
{
    const auto& tops = prefab.mTreeRoot->mChildren;
    if ( tops.size() != 1 or not tops[0]->IsEntity() )
        return std::nullopt;
    if ( prefab.mScene.HasComponent<PrefabInstanceComponent>( tops[0]->AsEntity() ) )
        return std::nullopt;
    return tops[0];
}

void LoadPrefabFile( Level& prefab, const path& absFile, Project& project )
{
    if ( not filesystem::is_regular_file( absFile ) )
        throw std::runtime_error( std::format( "No prefab file {}", absFile.string() ) );
    prefab.Load( absFile, project );
}
}


Ref<ProjectTreeNode> CopySubtreeInto( const Ref<ProjectTreeNode>& node,
                                      Scene& from,
                                      Level& to,
                                      map<Entity, Entity>& copied,
                                      std::optional<size_t> rootId )
{
    auto copy = CreateRef<ProjectTreeNode>( to.mNodeIDCounter );
    copy->mType = node->mType;
    if ( const auto entity = node->TryGetEntity(); entity and from.HasEntity( *entity ) )
    {
        const Entity made = rootId ? from.CopyEntityIntoWithId( to.mScene, *entity, *rootId )
                                   : from.CopyEntityInto( to.mScene, *entity );
        copied[*entity] = made;
        copy->mState = made;
    }
    else
    {
        copy->mState = node->mState;
    }
    for ( const auto& child : node->mChildren )
    {
        auto childCopy = CopySubtreeInto( child, from, to, copied );
        childCopy->mParent = copy;
        copy->mChildren.push_back( childCopy );
    }
    return copy;
}

void RemapEntityReferences( Scene& scene, const map<Entity, Entity>& copied )
{
    set<const void*> visited;
    for ( const auto& [_, entity] : copied )
    {
        if ( not scene.HasComponent<StateComponent>( entity ) )
            continue;
        const auto& state = scene.GetComponent<StateComponent>( entity ).mState;
        if ( state and state->is<Table>() )
            RemapTable( state->as<Table>(), copied, visited );
    }
}

Ref<ProjectTreeNode> InstantiatePrefab( Project& project,
                                        Level& level,
                                        const Ref<ProjectTreeNode>& parent,
                                        size_t index,
                                        const path& relPrefab,
                                        const PrefabPlacement& placement,
                                        std::optional<size_t> rootId )
{
    Level prefab;
    LoadPrefabFile( prefab, project.RootDir() / relPrefab, project );

    map<Entity, Entity> copied;
    Ref<ProjectTreeNode> root;
    if ( const auto single = SingleRoot( prefab ) )
    {
        root = CopySubtreeInto( *single, prefab.mScene, level, copied, rootId );
    }
    else
    {
        // Several things at the top: an entity to hold them, at the prefab's
        // origin, which is what places them.
        const Entity holder = rootId ? level.mScene.CreateEntityWithId( *rootId ) : level.mScene.CreateEntity();
        level.mScene.AddComponent<TagComponent>( holder, relPrefab.stem().string() );
        level.mScene.AddComponent<TransformComponent>( holder );
        root = CreateRef<ProjectTreeNode>( level.mNodeIDCounter );
        root->mType = ProjectTreeNodeType::Prefab;
        root->mState = holder;
        for ( const auto& top : prefab.mTreeRoot->mChildren )
        {
            auto copy = CopySubtreeInto( top, prefab.mScene, level, copied );
            copy->mParent = root;
            root->mChildren.push_back( copy );
        }
    }

    const Entity rootEntity = root->AsEntity();
    level.mScene.AddComponent<PrefabInstanceComponent>( rootEntity, relPrefab.generic_string() );
    auto& transform = level.mScene.GetComponent<TransformComponent>( rootEntity );
    if ( placement.mLocal )
    {
        transform.mPosition = placement.mLocal->mPosition;
        transform.mRotation = placement.mLocal->mRotation;
    }
    else if ( placement.mWorldPosition )
        transform.mPosition = *placement.mWorldPosition;

    InsertChild( parent, root, index );
    RemapEntityReferences( level.mScene, copied );
    SyncHierarchy( level.mScene, TreeRootOf( parent ) );

    // Under an entity, a point in the world is somewhere else locally.
    if ( placement.mWorldPosition and not placement.mLocal and ParentOf( level.mScene, rootEntity ) != INVALID_ENTITY )
    {
        Transform world = Transform::FromMatrix( ComputeWorldMatrix( level.mScene, rootEntity ) );
        world.mPosition = *placement.mWorldPosition;
        SetWorldTransform( level.mScene, rootEntity, world );
    }
    return root;
}

void SavePrefab( const Ref<ProjectTreeNode>& node, Scene& scene, const path& absFile, Project& project )
{
    Level prefab;
    prefab.mName = absFile.stem().string();
    prefab.mTreeRoot->mState = prefab.mName;

    map<Entity, Entity> copied;
    auto copy = CopySubtreeInto( node, scene, prefab, copied );
    InsertChild( prefab.mTreeRoot, copy, 0 );
    RemapEntityReferences( prefab.mScene, copied );

    // Where the entities are in the world, less where the first one is: the
    // prefab's origin is its first entity, wherever it was placed.
    vector<Entity> tops;
    TopEntities( node, tops );
    const vec3 origin = tops.empty() ? vec3( 0 ) : vec3( ComputeWorldMatrix( scene, tops.front() )[3] );
    for ( const Entity top : tops )
    {
        const Entity made = copied.at( top );
        if ( not prefab.mScene.HasComponent<TransformComponent>( made ) )
            continue;
        Transform world = Transform::FromMatrix( ComputeWorldMatrix( scene, top ) );
        world.mPosition -= origin;
        static_cast<Transform&>( prefab.mScene.GetComponent<TransformComponent>( made ) ) = world;
    }
    // The link to whatever this was an instance of stays on inner instances,
    // but the new prefab's own root is not an instance of anything.
    if ( node->IsEntity() and prefab.mScene.HasComponent<PrefabInstanceComponent>( copy->AsEntity() ) )
        prefab.mScene.RemoveComponent<PrefabInstanceComponent>( copy->AsEntity() );

    SyncHierarchy( prefab.mScene, prefab.mTreeRoot );
    filesystem::create_directories( absFile.parent_path() );
    prefab.Save( absFile, project );
}

vector<Ref<ProjectTreeNode>> FindPrefabInstances( const Level& level, const string& relPrefab )
{
    vector<Ref<ProjectTreeNode>> found;
    std::function<void( const Ref<ProjectTreeNode>& )> walk = [&]( const Ref<ProjectTreeNode>& node )
    {
        if ( const auto entity = node->TryGetEntity();
             entity and level.mScene.HasEntity( *entity ) and level.mScene.HasComponent<PrefabInstanceComponent>( *entity ) )
        {
            const string& prefab = level.mScene.GetComponent<PrefabInstanceComponent>( *entity ).mPrefab;
            if ( relPrefab.empty() or prefab == relPrefab )
            {
                found.push_back( node );
                if ( not relPrefab.empty() )
                    return;
            }
        }
        for ( const auto& child : node->mChildren )
            walk( child );
    };
    walk( level.mTreeRoot );
    return found;
}

vector<Entity> SpawnPrefab( Project& project, Scene& scene, const path& relPrefab, const vec3& position )
{
    Level prefab;
    LoadPrefabFile( prefab, project.RootDir() / relPrefab, project );

    map<Entity, Entity> copied;
    prefab.mScene.ForEachEntity( [&]( Entity entity )
    {
        copied[entity] = prefab.mScene.CopyEntityInto( scene, entity );
    } );
    // The links, which came across naming the prefab's ids.
    for ( const auto& [_, entity] : copied )
    {
        if ( not scene.HasComponent<HierarchyComponent>( entity ) )
            continue;
        auto& h = scene.GetComponent<HierarchyComponent>( entity );
        const auto mapped = [&]( Entity e ) { const auto it = copied.find( e ); return it != copied.end() ? it->second : INVALID_ENTITY; };
        h.mParent = mapped( h.mParent );
        vector<Entity> children;
        for ( const Entity child : h.mChildren )
            if ( const Entity m = mapped( child ); m != INVALID_ENTITY )
                children.push_back( m );
        h.mChildren = std::move( children );
    }

    Entity root;
    if ( const auto single = SingleRoot( prefab ) )
    {
        root = copied.at( ( *single )->AsEntity() );
    }
    else
    {
        root = scene.CreateEntity();
        scene.AddComponent<TagComponent>( root, relPrefab.stem().string() );
        scene.AddComponent<TransformComponent>( root );
        vector<Entity> tops;
        for ( const auto& top : prefab.mTreeRoot->mChildren )
            TopEntities( top, tops );
        for ( const Entity top : tops )
            SetParent( scene, copied.at( top ), root, /*keepWorld*/ false );
    }
    scene.AddComponent<PrefabInstanceComponent>( root, relPrefab.generic_string() );
    scene.GetComponent<TransformComponent>( root ).mPosition = position;
    RemapEntityReferences( scene, copied );
    UpdateWorldTransforms( scene );

    vector<Entity> spawned = { root };
    for ( const auto& [_, entity] : copied )
        if ( entity != root )
            spawned.push_back( entity );
    return spawned;
}

}
