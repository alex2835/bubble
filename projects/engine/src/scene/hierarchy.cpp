#include "engine/pch/pch.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/types/set.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
// Deeper than any real hierarchy; past it, the links are taken to be a loop
// that got into a file somehow, and the walk stops instead of the stack.
constexpr int cMaxDepth = 1024;

const HierarchyComponent* FindHierarchy( const Scene& scene, Entity entity )
{
    return scene.HasEntity( entity ) and scene.HasComponent<HierarchyComponent>( entity )
           ? &scene.GetComponent<HierarchyComponent>( entity ) : nullptr;
}

HierarchyComponent& EnsureHierarchy( Scene& scene, Entity entity )
{
    if ( scene.HasComponent<HierarchyComponent>( entity ) )
        return scene.GetComponent<HierarchyComponent>( entity );
    return scene.AddComponent<HierarchyComponent>( entity );
}

void UpdateSubtree( Scene& scene, Entity entity, const mat4& parentWorld, bool hasParent, int depth )
{
    if ( depth > cMaxDepth )
        return;

    // An entity without a transform - a script holder - passes its parent's
    // world on to its children.
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

size_t IndexInParent( const Scene& scene, Entity entity )
{
    const auto siblings = ChildrenOf( scene, ParentOf( scene, entity ) );
    const auto it = std::ranges::find( siblings, entity );
    return it == siblings.end() ? cAtEnd : size_t( it - siblings.begin() );
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

vector<Entity> Subtree( const Scene& scene, Entity entity )
{
    vector<Entity> out;
    std::function<void( Entity, int )> walk = [&]( Entity e, int depth )
    {
        if ( depth > cMaxDepth or not scene.HasEntity( e ) )
            return;
        out.push_back( e );
        for ( const Entity child : ChildrenOf( scene, e ) )
            walk( child, depth + 1 );
    };
    walk( entity, 0 );
    return out;
}

void AttachChild( Scene& scene, Entity child, Entity parent, size_t index )
{
    auto& children = EnsureHierarchy( scene, parent ).mChildren;
    children.insert( children.begin() + std::min( index, children.size() ), child );
    // After the parent's: adding a component may move the pool the other
    // reference points into.
    EnsureHierarchy( scene, child ).mParent = parent;
}

size_t DetachFromParent( Scene& scene, Entity child )
{
    const Entity parent = ParentOf( scene, child );
    size_t index = cAtEnd;
    if ( parent != INVALID_ENTITY and scene.HasEntity( parent ) )
    {
        auto& siblings = scene.GetComponent<HierarchyComponent>( parent ).mChildren;
        if ( const auto it = std::ranges::find( siblings, child ); it != siblings.end() )
        {
            index = size_t( it - siblings.begin() );
            siblings.erase( it );
        }
    }
    EnsureHierarchy( scene, child ).mParent = INVALID_ENTITY;
    return index;
}

bool SetParent( Scene& scene, Entity child, Entity parent, bool keepWorld, size_t index )
{
    if ( parent == INVALID_ENTITY )
        parent = scene.Root();
    if ( child == INVALID_ENTITY or not scene.HasEntity( child ) or child == scene.Root() )
        return false;
    if ( parent == INVALID_ENTITY or parent == child or not scene.HasEntity( parent ) or IsAncestor( scene, child, parent ) )
        return false;

    const mat4 world = ComputeWorldMatrix( scene, child );
    DetachFromParent( scene, child );
    AttachChild( scene, child, parent, index );
    if ( keepWorld )
        SetWorldTransform( scene, child, Transform::FromMatrix( world ) );
    return true;
}

Entity CreateChildEntity( Scene& scene, Entity parent )
{
    const Entity entity = scene.CreateEntity();
    scene.AddComponent<HierarchyComponent>( entity );
    if ( parent == INVALID_ENTITY )
        parent = scene.Root();
    if ( parent != INVALID_ENTITY )
        AttachChild( scene, entity, parent );
    return entity;
}

Entity CopySubtree( Scene& from, Entity entity, Scene& to, map<Entity, Entity>& copied, std::optional<size_t> topId )
{
    const vector<Entity> originals = Subtree( from, entity );
    for ( const Entity original : originals )
    {
        Entity made;
        if ( original == entity and topId )
            made = from.CopyEntityIntoWithId( to, original, *topId );
        else if ( &from == &to )
            made = from.CopyEntity( original );
        else
            made = from.CopyEntityInto( to, original );
        copied[original] = made;
    }
    // The links came across naming the originals.
    for ( const Entity original : originals )
    {
        const Entity made = copied.at( original );
        auto& h = EnsureHierarchy( to, made );
        const auto parent = copied.find( h.mParent );
        h.mParent = original == entity or parent == copied.end() ? INVALID_ENTITY : parent->second;
        vector<Entity> children;
        for ( const Entity child : h.mChildren )
            if ( const auto it = copied.find( child ); it != copied.end() )
                children.push_back( it->second );
        h.mChildren = std::move( children );
    }
    RemapEntityReferences( to, copied );
    return copied.at( entity );
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

void UpdateWorldTransforms( Scene& scene )
{
    // From the root; and from anything hanging from nothing, which the tree
    // does not show but which still has a place in the world.
    vector<Entity> tops;
    scene.ForEachEntity( [&]( Entity entity )
    {
        const Entity parent = ParentOf( scene, entity );
        if ( parent == INVALID_ENTITY or not scene.HasEntity( parent ) )
            tops.push_back( entity );
    } );
    for ( const Entity top : tops )
        UpdateSubtree( scene, top, mat4( 1.0f ), false, 0 );
}

}
