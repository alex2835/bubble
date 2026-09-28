#pragma once
// Only the base. Pulling components.hpp in here put every component header
// under every file that touches the scene, so editing one component rebuilt
// half the engine. A file that uses a component includes that component.
#include "engine/scene/components/component_base.hpp"
#include "engine/types/array.hpp"
#include <entt/entity/registry.hpp>
#include <format>
#include <span>
#include <stdexcept>

namespace bubble
{
// The entities of a level or a prefab. They form one tree: every entity has a
// HierarchyComponent, and every one but the root has a parent - the root
// being the level itself (or the prefab), which the Entities window shows at
// the top. See engine/scene/hierarchy.hpp.
//
// Storage is an EnTT registry. Components are reached by type, or by their
// ComponentTypeId - for the inspector, the file format, the undo commands and
// Lua's for_each_entity, which do not name the type. The id API knows the
// types ComponentManager registered.
//
// Component addresses are stable while other entities get the same type:
// EnTT stores components in pages and never moves one to make room. Removing
// one moves the last of its type into the hole, though, so a pointer is good
// only until the next removal of that type.
class Scene
{
public:
    using Registry = entt::basic_registry<Entity>;
    using Storage = Registry::common_type;

    Scene();

    Entity Root() const { return mRoot; }
    void SetRoot( Entity root ) { mRoot = root; }

    /// Entities
    Entity CreateEntity();
    // Exactly `id`, which must not be alive: a file's entities and an undone
    // deletion come back under the ids other things refer to them by.
    Entity CreateEntity( Entity id );
    // False for Entity::Null and for a handle whose entity is gone.
    bool HasEntity( Entity entity ) const { return mRegistry.valid( entity ); }
    void RemoveEntity( Entity entity );
    // Ids that are not alive are skipped.
    void RemoveEntities( std::span<const Entity> entities );
    size_t EntityCount() const;

    // Every registered component of `entity` copied onto a new entity of
    // `target` - which may be this scene - or onto `id` there.
    Entity CopyEntity( Entity entity, Scene& target, Entity id = Entity::Null );

    /// Components by type
    // Replaces the component if the entity has one already.
    template <ComponentType Component, typename... Args>
    Component& AddComponent( Entity entity, Args&&... args );

    // Throws when the entity does not have it.
    template <ComponentType Component>
    Component& GetComponent( Entity entity );
    template <ComponentType Component>
    const Component& GetComponent( Entity entity ) const;

    // Null when the entity does not have it.
    template <ComponentType Component>
    Component* TryGetComponent( Entity entity );
    template <ComponentType Component>
    const Component* TryGetComponent( Entity entity ) const;

    template <ComponentType Component>
    bool HasComponent( Entity entity ) const { return mRegistry.all_of<Component>( entity ); }

    template <ComponentType Component>
    void RemoveComponent( Entity entity ) { mRegistry.remove<Component>( entity ); }

    /// Components by id
    // A default made component, returned for filling in. Throws if the
    // entity has one.
    void* AddComponent( Entity entity, ComponentTypeId componentId );
    void* TryGetComponent( Entity entity, ComponentTypeId componentId );
    const void* TryGetComponent( Entity entity, ComponentTypeId componentId ) const;
    bool HasComponent( Entity entity, ComponentTypeId componentId ) const;
    void RemoveComponent( Entity entity, ComponentTypeId componentId );
    // The component of `entity` onto `targetEntity` in `target`, which must
    // not have that component yet.
    void CopyComponent( Entity entity, ComponentTypeId componentId, Scene& target, Entity targetEntity );
    // What the entity has, in id order - the order the inspector lists them in.
    vector<ComponentTypeId> ComponentsOf( Entity entity ) const;

    /// Iteration
    // func( Entity, Components&... ) for every entity that has all of them.
    template <ComponentType... Components, typename F>
    void ForEach( F&& func );
    template <ComponentType... Components, typename F>
    void ForEach( F&& func ) const;

    // func( Entity, std::span<void* const> ) for every entity that has all of
    // `componentIds`, the components in that order. Walks a snapshot, so
    // func may add and remove entities.
    template <typename F>
    void ForEach( std::span<const ComponentTypeId> componentIds, F&& func );

    template <typename F>
    void ForEachEntity( F&& func ) const;

    // func( Entity, const void* ) for each entity with the component.
    template <typename F>
    void ForEachComponent( ComponentTypeId componentId, F&& func ) const;

private:
    // Null when no entity of this scene ever had the type.
    Storage* FindStorage( ComponentTypeId componentId );
    const Storage* FindStorage( ComponentTypeId componentId ) const;
    Storage& EnsureStorage( ComponentTypeId componentId );
    void RequireEntity( Entity entity, std::string_view what ) const;

    Registry mRegistry;
    Entity mRoot = Entity::Null;
};


// ------------------------ Components by type ------------------------

template <ComponentType Component, typename... Args>
Component& Scene::AddComponent( Entity entity, Args&&... args )
{
    RequireEntity( entity, "AddComponent" );
    return mRegistry.emplace_or_replace<Component>( entity, std::forward<Args>( args )... );
}

template <ComponentType Component>
Component& Scene::GetComponent( Entity entity )
{
    if ( Component* component = TryGetComponent<Component>( entity ) )
        return *component;
    throw std::runtime_error( std::format( "GetComponent: entity {} doesn't have component {}",
                                           (size_t)entity, Component::ID() ) );
}

template <ComponentType Component>
const Component& Scene::GetComponent( Entity entity ) const
{
    return const_cast<Scene*>( this )->GetComponent<Component>( entity );
}

template <ComponentType Component>
Component* Scene::TryGetComponent( Entity entity )
{
    return mRegistry.try_get<Component>( entity );
}

template <ComponentType Component>
const Component* Scene::TryGetComponent( Entity entity ) const
{
    return mRegistry.try_get<Component>( entity );
}


// ------------------------ Iteration ------------------------

template <ComponentType... Components, typename F>
void Scene::ForEach( F&& func )
{
    mRegistry.view<Components...>().each( [&func]( Entity entity, Components&... components )
    {
        func( entity, components... );
    } );
}

template <ComponentType... Components, typename F>
void Scene::ForEach( F&& func ) const
{
    // A const registry has no storage to make for a type nobody has had yet;
    // the view over it is then not valid, and there is nothing to walk.
    const auto view = mRegistry.view<const Components...>();
    if ( not view )
        return;
    view.each( [&func]( Entity entity, const Components&... components )
    {
        func( entity, components... );
    } );
}

template <typename F>
void Scene::ForEach( std::span<const ComponentTypeId> componentIds, F&& func )
{
    vector<Storage*> storages;
    const Storage* smallest = nullptr;
    for ( const ComponentTypeId id : componentIds )
    {
        Storage* storage = FindStorage( id );
        if ( not storage )
            return; // nobody has it
        storages.push_back( storage );
        if ( not smallest or storage->size() < smallest->size() )
            smallest = storage;
    }
    if ( not smallest )
        return;

    const vector<Entity> entities( smallest->begin(), smallest->end() );
    vector<void*> components( storages.size() );
    for ( const Entity entity : entities )
    {
        bool all = true;
        for ( size_t i = 0; i < storages.size() and all; i++ )
        {
            all = storages[i]->contains( entity );
            if ( all )
                components[i] = storages[i]->value( entity );
        }
        if ( all )
            func( entity, std::span<void* const>( components ) );
    }
}

template <typename F>
void Scene::ForEachEntity( F&& func ) const
{
    for ( const auto [entity] : mRegistry.storage<Entity>()->each() )
        func( entity );
}

template <typename F>
void Scene::ForEachComponent( ComponentTypeId componentId, F&& func ) const
{
    const Storage* storage = FindStorage( componentId );
    if ( not storage )
        return;
    for ( const Entity entity : *storage )
        func( entity, storage->value( entity ) );
}

}
