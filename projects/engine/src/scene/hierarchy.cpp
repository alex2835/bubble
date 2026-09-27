#include "engine/pch/pch.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/project/project_tree.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/types/map.hpp"

namespace bubble
{
namespace
{
// Deeper than any real hierarchy; past it, the links are taken to be a loop
// that got into a file somehow, and the walk stops instead of the stack.
constexpr int cMaxDepth = 1024;

HierarchyComponent* FindHierarchy( Scene& scene, Entity entity )
{
    return scene.HasComponent<HierarchyComponent>( entity ) ? &scene.GetComponent<HierarchyComponent>( entity ) : nullptr;
}

const HierarchyComponent* FindHierarchy( const Scene& scene, Entity entity )
{
    return scene.HasComponent<HierarchyComponent>( entity ) ? &scene.GetComponent<HierarchyComponent>( entity ) : nullptr;
}

// A component nobody needs any more - no parent, no children - goes.
void DropIfUnused( Scene& scene, Entity entity )
{
    if ( const auto* h = FindHierarchy( scene, entity ); h and h->mParent == INVALID_ENTITY and h->mChildren.empty() )
        scene.RemoveComponent<HierarchyComponent>( entity );
}

HierarchyComponent& EnsureHierarchy( Scene& scene, Entity entity )
{
    if ( auto* h = FindHierarchy( scene, entity ) )
        return *h;
    return scene.AddComponent<HierarchyComponent>( entity );
}

void UpdateSubtree( Scene& scene, Entity entity, const mat4& parentWorld, bool hasParent, int depth )
{
    if ( depth > cMaxDepth )
        return;

    mat4 world = parentWorld;
    if ( scene.HasComponent<TransformComponent>( entity ) )
    {
        auto& transform = scene.GetComponent<TransformComponent>( entity );
        if ( hasParent )
        {
            transform.mWorldMatrix = parentWorld * transform.TransformMat();
            transform.mWorld = Transform::FromMatrix( transform.mWorldMatrix );
        }
        else
        {
            transform.mWorldMatrix = transform.TransformMat();
            transform.mWorld = transform;
        }
        world = transform.mWorldMatrix;
    }

    const auto* hierarchy = FindHierarchy( scene, entity );
    if ( not hierarchy )
        return;
    // Copied: a child's update must not be looking into this pool while it
    // writes its own component, and the list is short.
    const vector<Entity> children = hierarchy->mChildren;
    for ( const Entity child : children )
        if ( scene.HasEntity( child ) )
            UpdateSubtree( scene, child, world, true, depth + 1 );
}

bool IsRoot( const Scene& scene, Entity entity )
{
    const auto* h = FindHierarchy( scene, entity );
    return not h or h->mParent == INVALID_ENTITY or not scene.HasEntity( h->mParent );
}
}


Entity ParentOf( const Scene& scene, Entity entity )
{
    const auto* h = FindHierarchy( scene, entity );
    return h ? h->mParent : INVALID_ENTITY;
}

std::span<const Entity> ChildrenOf( const Scene& scene, Entity entity )
{
    const auto* h = FindHierarchy( scene, entity );
    return h ? std::span<const Entity>( h->mChildren ) : std::span<const Entity>();
}

bool IsAncestor( const Scene& scene, Entity ancestor, Entity entity )
{
    Entity current = ParentOf( scene, entity );
    for ( int depth = 0; current != INVALID_ENTITY and depth < cMaxDepth; depth++ )
    {
        if ( current == ancestor )
            return true;
        current = ParentOf( scene, current );
    }
    return false;
}

mat4 ComputeWorldMatrix( const Scene& scene, Entity entity )
{
    mat4 world = scene.HasComponent<TransformComponent>( entity )
                 ? scene.GetComponent<TransformComponent>( entity ).TransformMat()
                 : mat4( 1.0f );
    Entity parent = ParentOf( scene, entity );
    for ( int depth = 0; parent != INVALID_ENTITY and scene.HasEntity( parent ) and depth < cMaxDepth; depth++ )
    {
        if ( scene.HasComponent<TransformComponent>( parent ) )
            world = scene.GetComponent<TransformComponent>( parent ).TransformMat() * world;
        parent = ParentOf( scene, parent );
    }
    return world;
}

void SetWorldTransform( Scene& scene, Entity entity, const Transform& world )
{
    if ( not scene.HasComponent<TransformComponent>( entity ) )
        return;
    const Entity parent = ParentOf( scene, entity );
    Transform local = world;
    if ( parent != INVALID_ENTITY and scene.HasEntity( parent ) )
        local = Transform::FromMatrix( glm::inverse( ComputeWorldMatrix( scene, parent ) ) * world.TransformMat() );
    auto& transform = scene.GetComponent<TransformComponent>( entity );
    transform.mPosition = local.mPosition;
    transform.mRotation = local.mRotation;
    transform.mScale = local.mScale;
}

bool SetParent( Scene& scene, Entity child, Entity parent, bool keepWorld )
{
    if ( child == INVALID_ENTITY or not scene.HasEntity( child ) )
        return false;
    if ( parent != INVALID_ENTITY and ( parent == child or not scene.HasEntity( parent ) or IsAncestor( scene, child, parent ) ) )
        return false;

    const Entity oldParent = ParentOf( scene, child );
    if ( oldParent == parent )
        return true;

    const mat4 world = ComputeWorldMatrix( scene, child );

    if ( oldParent != INVALID_ENTITY and scene.HasEntity( oldParent ) )
    {
        auto& siblings = scene.GetComponent<HierarchyComponent>( oldParent ).mChildren;
        std::erase( siblings, child );
        DropIfUnused( scene, oldParent );
    }
    if ( parent != INVALID_ENTITY )
        EnsureHierarchy( scene, parent ).mChildren.push_back( child );
    // After the parent's: adding a component may move the pool the other
    // reference points into.
    EnsureHierarchy( scene, child ).mParent = parent;
    DropIfUnused( scene, child );

    if ( keepWorld )
        SetWorldTransform( scene, child, Transform::FromMatrix( world ) );
    return true;
}

void UpdateWorldTransforms( Scene& scene )
{
    // Roots with a transform, and the transformless ones that have children
    // - a script entity grouping others, say - which pass the identity down.
    vector<Entity> roots;
    scene.ForEach<TransformComponent>( [&]( Entity entity, const TransformComponent& )
    {
        if ( IsRoot( scene, entity ) )
            roots.push_back( entity );
    } );
    scene.ForEach<HierarchyComponent>( [&]( Entity entity, const HierarchyComponent& h )
    {
        if ( not scene.HasComponent<TransformComponent>( entity ) and not h.mChildren.empty() and IsRoot( scene, entity ) )
            roots.push_back( entity );
    } );
    for ( const Entity root : roots )
        UpdateSubtree( scene, root, mat4( 1.0f ), false, 0 );
}

void SyncHierarchy( Scene& scene, const Ref<ProjectTreeNode>& root )
{
    struct Links
    {
        Entity mParent = INVALID_ENTITY;
        vector<Entity> mChildren;
    };
    hash_map<Entity, Links> links;
    vector<Entity> order;

    std::function<void( const Ref<ProjectTreeNode>&, Entity )> walk = [&]( const Ref<ProjectTreeNode>& node, Entity above )
    {
        Entity next = above;
        if ( const auto entity = node->TryGetEntity(); entity and scene.HasEntity( *entity ) )
        {
            links[*entity].mParent = above;
            if ( above != INVALID_ENTITY )
                links[above].mChildren.push_back( *entity );
            order.push_back( *entity );
            next = *entity;
        }
        for ( const auto& child : node->mChildren )
            walk( child, next );
    };
    if ( root )
        walk( root, INVALID_ENTITY );

    for ( const Entity entity : order )
    {
        const Links& want = links[entity];
        const bool needed = want.mParent != INVALID_ENTITY or not want.mChildren.empty();
        auto* have = FindHierarchy( scene, entity );
        if ( not needed )
        {
            if ( have )
                scene.RemoveComponent<HierarchyComponent>( entity );
            continue;
        }
        if ( not have )
            have = &scene.AddComponent<HierarchyComponent>( entity );
        if ( have->mParent != want.mParent )
            have->mParent = want.mParent;
        if ( have->mChildren != want.mChildren )
            have->mChildren = want.mChildren;
    }
}

}
