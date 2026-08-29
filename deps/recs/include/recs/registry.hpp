#pragma once
#include <unordered_map>
#include <unordered_set>
#include <bitset>
#include <set>
#include <string_view>
#include <format>
#include <string>
#include <array>
#include <tuple>
#include <ranges>
#include <span>
#include <vector>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/entity.hpp"
#include "recs/pool.hpp"
#include "recs/view.hpp"

namespace recs
{
class RECS_EXPORT Registry
{
public:
    Registry() = default;
    Registry( const Registry& ) = default;
    Registry& operator=( const Registry& ) = default;
    Registry( Registry&& ) = default;
    Registry& operator=( Registry&& ) = default;

    Entity CreateEntity();
    Entity CreateEntityWithId( size_t id );
    Entity GetEntityById( size_t id );

    // True if the entity is live in this registry, whatever components it
    // holds. RemoveEntity and GetComponent assert or throw on anything else,
    // so callers holding a handle of unknown age should ask first.
    bool HasEntity( Entity entity ) const noexcept
    {
        return mEntitiesComponentTypeIds.contains( entity );
    }

    void RemoveEntity( Entity entity );

    // Batch erase. One compaction pass per pool instead of one tail shift per
    // entity, which is what makes deleting a large selection O(n) not O(n*m).
    void RemoveEntities( std::span<const Entity> entities );
    Entity CopyEntity( Entity entity );
    Entity CopyEntityInto( Registry& targetRegistry, Entity entity );
    Entity CopyEntityIntoWithId( Registry& targetRegistry, Entity entity, size_t targetId );

    // Component types API
    template <ComponentType Component>
    Registry& AddComponent();

    template <ComponentType Component, typename ...Args>
    Component& AddComponent( Entity entity, Args&& ...args );

    template <ComponentType Component>
    Component& GetComponent( Entity entity );

    template <ComponentType Component>
    const Component& GetComponent( Entity entity ) const;

    template <ComponentType ...Components>
    std::tuple<Components&...> GetComponents( Entity entity );

    template <ComponentType Component>
    bool HasComponent( Entity entity ) const;

    template <ComponentType ...Components>
    bool HasComponents( Entity entity ) const;

    template <ComponentType Component>
    void RemoveComponent( Entity entity );
    
    // ComponentTypeIds API
    const std::set<ComponentTypeId>& AllComponentTypeIds() const;
    const std::set<ComponentTypeId>& EntityComponentTypeIds( Entity entity ) const;
    void EntityAddComponentId( Entity entity, ComponentTypeId componentId );
    void EntityRemoveComponentId( Entity entity, ComponentTypeId componentId );

    // For each entities
    template <ComponentType ...Components, typename F>
    void ForEach( F&& func );

    template <ComponentType ...Components, typename F>
    void ForEach( F&& func ) const;

    // For each on provided components Ids
    // If id is INVALID_COMPONENT_TYPE_ID(=-1) this component will be
    // skipped while iteration and filled just with nullptr
    template <size_t SIZE, typename F>
    void RuntimeForEach( const std::array<ComponentTypeId, SIZE>& components, F&& func );

    template <typename F>
    void ForEachEntity( F&& func );

    template <typename F>
    void ForEachEntity( F&& func ) const;

    template <typename F>
    void ForEachEntityComponentRaw( Entity entity, F&& func );

    // Lazy view over every entity holding all of Components. A component type
    // that is not registered yields an empty view rather than throwing.
    template <ComponentType ...Components>
    View<Components...> GetView();

    size_t Size() const noexcept
    {
        return mEntitiesComponentTypeIds.size();
    }

private:
    Pool& GetPool( ComponentTypeId componentId );

    template <ComponentType Component>
    Component& EntityGetComponent( Entity entity );

    void EntityAddComponent( Entity entity, ComponentTypeId component );
    bool EntityHasComponent( Entity entity, ComponentTypeId component ) const;
    void EntityRemoveComponent( Entity entity, ComponentTypeId component );
    std::set<ComponentTypeId>& GetEntityComponentsIds( Entity entity );

    // For each
    template <ComponentType ...Components, typename F>
    void ForEachTuple( F&& func );

    template <ComponentType ...Components, typename F>
    void ForEachTuple( F&& func ) const;

    template <typename ...Args, size_t Size, size_t ...Is>
    std::tuple<const Args&...> MakeTupleFromPoolsAndIndiciesConst( const std::array<const Pool*, Size>& pools,
                                                                   const std::array<size_t, Size>& indicies,
                                                                   std::index_sequence<Is...> ) const
    {
        return std::forward_as_tuple( pools[Is]->template Get<Args>( indicies[Is] )... );
    }

    Pool& GetComponentPool( ComponentTypeId id );
    const Pool& GetComponentPool( ComponentTypeId id ) const;

protected:
    size_t mEntityCounter = 1;
    std::set<ComponentTypeId> mComponents;
    std::unordered_map<Entity, std::set<ComponentTypeId>> mEntitiesComponentTypeIds;
    std::unordered_map<ComponentTypeId, Pool> mPools;
};


// ------------------------ Component IDs ------------------------

template <ComponentType ...Components>
std::array<ComponentTypeId, sizeof...( Components )> GetComponentsTypeId()
{
    return { Components::ID()... };
}

template <ComponentType Component>
Component& Registry::EntityGetComponent( Entity entity )
{
    Pool& pool = GetComponentPool( Component::ID() );
    return pool.Get<Component>( entity );
}


// ------------------------ Registry ------------------------

template <ComponentType Component>
Registry& Registry::AddComponent()
{
    if ( mComponents.emplace( Component::ID() ).second )
        mPools.emplace( Component::ID(), Pool::CreatePool<Component>() );
    return *this;
}

template <ComponentType Component, typename ...Args>
Component& Registry::AddComponent( Entity entity, Args&& ...args )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "AddComponent: Invalid entity" );

    if ( mComponents.emplace( Component::ID() ).second )
        mPools.emplace( Component::ID(), Pool::CreatePool<Component>() );

    if ( EntityHasComponent( entity, Component::ID() ) )
    {
        auto& component = EntityGetComponent<Component>( entity );
        component = Component( std::forward<Args>( args )... );
        return component;
    }
    else
    {
        EntityAddComponent( entity, Component::ID() );
        Pool& pool = mPools[Component::ID()];
        auto& component = pool.Push<Component>( entity, std::forward<Args>( args )... );
        return component;
    }
}

template <ComponentType Component>
Component& Registry::GetComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "GetComponent: Invalid entity" );

    // The pool lookup already answers "does this entity have the component",
    // so a HasComponent pre-check would just repeat the work.
    Pool& pool = GetComponentPool( Component::ID() );
    if ( void* raw = pool.GetRaw( entity ) )
        return *static_cast<Component*>( raw );

    throw std::runtime_error( std::format( "GetComponent: Entity {} doesn't have component {}", (size_t)entity, Component::ID() ) );
}

template <ComponentType Component>
const Component& Registry::GetComponent( Entity entity ) const
{
    return const_cast<Registry*>( this )->GetComponent<Component>( entity );
}

template <ComponentType ...Components>
std::tuple<Components&...> Registry::GetComponents( Entity entity )
{
    // Each GetComponent validates on its own; pre-checking here would double
    // every lookup.
    return std::forward_as_tuple( GetComponent<Components>( entity )... );
}

template <ComponentType Component>
bool Registry::HasComponent( Entity entity ) const
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "HasComponent: Invalid entity" );

    return EntityHasComponent( entity, Component::ID() );
}

template <ComponentType ...Components>
bool Registry::HasComponents( Entity entity ) const
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "HasComponents: Invalid entity" );

    return ( EntityHasComponent( entity, Components::ID() ) && ... );
}


template <ComponentType Component>
void Registry::RemoveComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "RemoveComponent: Invalid entity" );

    //ComponentTypeId component = GetComponentTypeId<Component>();
    if ( EntityHasComponent( entity, Component::ID() ) )
    {
        Pool& pool = mPools[Component::ID()];
        pool.Remove( entity );
        EntityRemoveComponent( entity, Component::ID() );
    }
}



// ------------------------ ForEach ------------------------

template <typename F>
void Registry::ForEachEntityComponentRaw( Entity entity, F&& func )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "ForEachEntityComponentRaw: Invalid entity" );

    auto entityIter = mEntitiesComponentTypeIds.find( entity );
    if ( entityIter == mEntitiesComponentTypeIds.end() )
        return;
    const auto& components = entityIter->second;
    for ( const auto id : mComponents )
    {
        if ( components.count( id ) )
        {
            Pool& pool = mPools[id];
            func( id, pool.GetRaw( entity ) );
        }
    }
}

template <ComponentType ...Components, typename F>
void Registry::ForEach( F&& func ) const
{
    // func is invoked once per entity, so it must not be forwarded (moved) here.
    ForEachTuple<Components...>( [&func]( Entity entity, std::tuple<const Components&...> components )
    {
        std::apply( func, std::tuple_cat( std::make_tuple( entity ), components ) );
    } );
}

template <ComponentType ...Components, typename F>
void Registry::ForEach( F&& func )
{
    // func is invoked once per entity, so it must not be forwarded (moved) here.
    ForEachTuple<Components...>( [&func]( Entity entity, std::tuple<Components&...> components )
    {
        std::apply( func, std::tuple_cat( std::make_tuple( entity ), components ) );
    } );
}

template <typename F>
void Registry::ForEachEntity( F&& func ) const
{
    for ( const auto& [entity, _] : mEntitiesComponentTypeIds )
        func( entity );
}

template <typename F>
void Registry::ForEachEntity( F&& func )
{
    std::as_const( *this ).ForEachEntity( std::forward<F>( func ) );
}

template <ComponentType ...Components, typename F>
void Registry::ForEachTuple( F&& func ) const
{
    // A component type nobody has registered yet simply has no entities, so the
    // intersection is empty. That is a normal result, not an error.
    if ( !( mComponents.contains( Components::ID() ) && ... ) )
        return;

    constexpr auto size = sizeof...( Components );
    std::array<const Pool*, size> pools;

    auto componentTypes = GetComponentsTypeId<Components...>();
    for ( size_t i = 0; i < componentTypes.size(); i++ )
        pools[i] = &GetComponentPool( componentTypes[i] );

    size_t maxId = 0;
    std::array<size_t, size> indicies;
    indicies.fill( 0u );
    while ( true )
    {
        for ( size_t i = 0; i < size; i++ )
        {
            if ( indicies[i] >= pools[i]->Size() )
                return;

            if ( maxId < pools[i]->mEntities[indicies[i]].mId )
                maxId = pools[i]->mEntities[indicies[i]].mId;
        }

        bool skip = false;
        for ( size_t i = 0; i < size; i++ )
        {
            indicies[i] = pools[i]->AdvanceTo( indicies[i], maxId );
            if ( indicies[i] >= pools[i]->Size() )
                return;

            if ( pools[i]->mEntities[indicies[i]].mId > maxId )
            {
                skip = true;
                break;
            }
        }
        if ( skip )
            continue;

        func( Entity( maxId ), MakeTupleFromPoolsAndIndiciesConst<Components...>( pools, indicies, std::make_index_sequence<size>{} ) );

        for ( size_t i = 0; i < size; i++ )
            indicies[i]++;
    }
}

template <size_t SIZE, typename F>
void Registry::RuntimeForEach( const std::array<ComponentTypeId, SIZE>& componentIds, F&& func )
{
    // valid set of components
    std::bitset<SIZE> validBS;
    for ( size_t i = 0; i < SIZE; i++ )
        validBS.set( i, componentIds[i] != INVALID_COMPONENT_TYPE_ID );

    // With no valid component id there is nothing to intersect and nothing to
    // advance, so the loop below would never terminate.
    if ( validBS.none() )
        return;

    // pools
    std::array<Pool*, SIZE> pools;
    pools.fill( nullptr );
    for ( size_t i = 0; i < SIZE; i++ )
    {
        if ( validBS.test( i ) )
            pools[i] = &GetComponentPool( componentIds[i] );
    }

    std::array<size_t, SIZE> indicies;
    indicies.fill( 0 );

    size_t maxId = 0;
    while ( true )
    {
        // update max entity id
        for ( size_t i = 0; i < SIZE; i++ )
        {
            if ( not validBS.test( i ) )
                continue;

            const auto& pool = pools[i];
            auto& entityIndex = indicies[i];

            if ( entityIndex >= pool->Size() )
                return;

            const auto& poolEntities = pool->mEntities;
            if ( maxId < poolEntities[entityIndex].mId )
                maxId = poolEntities[entityIndex].mId;
        }

        // find indices in pools of max entity id
        bool skip = false;
        for ( size_t i = 0; i < SIZE; i++ )
        {
            if ( not validBS.test( i ) )
                continue;

            const auto& pool = pools[i];
            const auto& poolEntities = pool->mEntities;
            auto& entityIndex = indicies[i];

            entityIndex = pool->AdvanceTo( entityIndex, maxId );
            if ( entityIndex >= pool->Size() )
                return;

            if ( poolEntities[entityIndex].mId > maxId )
            {
                skip = true;
                break;
            }
        }
        if ( skip )
            continue;


        // Fill components pointers
        std::array<void*, SIZE> components;
        components.fill( nullptr );

        for ( size_t i = 0; i < SIZE; i++ )
        {
            if ( validBS.test( i ) )
                components[i] = pools[i]->GetElemAddress( indicies[i] );
        }

        // Call functor callback
        func( Entity( maxId ), components );

        for ( size_t i = 0; i < SIZE; i++ )
        {
            if ( validBS.test( i ) )
                indicies[i]++;
        }
    }
}

template <ComponentType ...Components, typename F>
void Registry::ForEachTuple( F&& func )
{
    std::as_const( *this ).ForEachTuple<Components...>(
        [&func]( Entity entity, std::tuple<const Components&...> components )
        {
            [&]<size_t ...Is>( std::index_sequence<Is...> )
            {
                func( entity, std::tuple<Components&...>(
                    const_cast<Components&>( std::get<Is>( components ) )...
                ) );
            }( std::make_index_sequence<sizeof...( Components )>{} );
        }
    );
}


// ------------------------ View ------------------------

template <ComponentType ...Components>
View<Components...> Registry::GetView()
{
    constexpr size_t size = sizeof...( Components );

    std::array<Pool*, size> pools;
    pools.fill( nullptr );

    const auto componentTypes = GetComponentsTypeId<Components...>();
    for ( size_t i = 0; i < size; i++ )
    {
        const auto iter = mPools.find( componentTypes[i] );
        if ( iter != mPools.end() )
            pools[i] = &iter->second;
    }

    // A null pool leaves the view empty - see View::Iterator's constructor.
    return View<Components...>( pools );
}


}