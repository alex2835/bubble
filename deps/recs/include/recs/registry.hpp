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
#include <cassert>
#include <stdexcept>
#include "recs/impex.hpp"
#include "recs/utils.hpp"
#include "recs/entity.hpp"
#include "recs/pool.hpp"

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
    void RemoveEntity( Entity entity );
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
    const std::set<ComponentTypeId>& AllComponentTypeIds();
    const std::set<ComponentTypeId>& EntityComponentTypeIds( Entity entity );
    void EntityAddComponentId( Entity entity, ComponentTypeId componentId );
    void EntityRemoveComponentId( Entity entity, ComponentTypeId componentId );

    // For each entities
    template <ComponentType ...Components, typename F>
    void ForEach( F&& func );

    // For each on provided components Ids
    // If id is INVALID_COMPONENT_TYPE_ID(=-1) this component will be 
    // skipped while iteration and filled just with nullptr 
    template <size_t SIZE, typename F>
    void RuntimeForEach( const std::array<ComponentTypeId, SIZE>& components, F&& func );

    template <typename F>
    void ForEachEntity( F&& func );

    template <typename F>
    void ForEachEntityComponentRaw( Entity entity, F&& func );

    size_t Size()
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
    
    template <typename ...Args, size_t Size, size_t ...Is>
    std::tuple<Args&...> MakeTupleFromPoolsAndIndicies( const std::array<Pool*, Size>& pools, 
                                                        const std::array<size_t, Size>& indicies,
                                                        std::index_sequence<Is...> )
    {
        return std::forward_as_tuple( pools[Is]->template Get<Args>( indicies[Is] )... );
    }
    Pool& GetComponentPool( ComponentTypeId id );

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
    if ( !HasComponent<Component>( entity ) )
        throw std::runtime_error( std::format( "GetComponent: Entity {} doesn't have component {}", (size_t)entity, Component::ID() ) );

    Pool& pool = GetComponentPool( Component::ID() );
    return pool.Get<Component>( entity );
}

template <ComponentType Component>
const Component& Registry::GetComponent( Entity entity ) const
{
    return const_cast<Registry*>( this )->GetComponent<Component>( entity );
}

template <ComponentType ...Components>
std::tuple<Components&...> Registry::GetComponents( Entity entity )
{
    if ( !HasComponents<Components...>( entity ) )
        throw std::runtime_error( std::format( "GetComponents: Entity {} doesn't have required components", (size_t)entity ) );

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

    return ( EntityHasComponent( entity, GetComponentTypeId<Components>() ) && ... );
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

    const auto& components = mEntitiesComponentTypeIds[entity];
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
void Registry::ForEach( F&& func )
{
    ForEachTuple<Components...>( [&func]( Entity entity, std::tuple<Components&...> components )
    {
        std::apply( std::forward<F>( func ), std::tuple_cat( std::make_tuple( entity ), components ) );
    } );
}

template <typename F>
void Registry::ForEachEntity( F&& func )
{
    for ( const auto& [entity, _] : mEntitiesComponentTypeIds )
        func( entity );
}

template <ComponentType ...Components, typename F>
void Registry::ForEachTuple( F&& func )
{
    if ( !( mComponents.contains( Components::ID() ) || ... ) )
        throw std::runtime_error( "ForEachTuple: Invalid components - registry doesn't contain required component types" );

    constexpr auto size = sizeof...( Components );
    std::array<Pool*, size> pools;

    auto componentTypes = GetComponentsTypeId<Components...>();
    for ( size_t i = 0; i < componentTypes.size(); i++ )
        pools[i] = &GetComponentPool( componentTypes[i] );

    size_t maxId = 0;
    std::array<size_t, size> indicies;
    indicies.fill( 0u );
    while ( true )
    {
        for ( int i = 0; i < size; i++ )
        {
            if ( indicies[i] >= pools[i]->Size() )
                return;

            if ( maxId < pools[i]->mEntities[indicies[i]].mId )
                maxId = pools[i]->mEntities[indicies[i]].mId;
        }

        bool skip = false;
        for ( int i = 0; i < size; i++ )
        {
            while ( pools[i]->mEntities[indicies[i]].mId < maxId )
            {
                if ( pools[i]->mEntities[indicies[i]].mId > maxId )
                {
                    skip = true;
                    break;
                }

                indicies[i]++;
                if ( indicies[i] >= pools[i]->Size() )
                    return;
            }
        }
        if ( skip ) 
            continue;

        func( Entity( maxId ), MakeTupleFromPoolsAndIndicies<Components...>( pools, indicies, std::make_index_sequence<size>{} ) );

        for ( int i = 0; i < size; i++ )
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

            while ( poolEntities[entityIndex].mId < maxId )
            {
                if ( poolEntities[entityIndex].mId > maxId )
                {
                    skip = true;
                    break;
                }

                entityIndex++;
                if ( entityIndex >= pool->Size() )
                    return;
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

        for ( int i = 0; i < componentIds.size(); i++ )
            indicies[i]++;
    }
}


}