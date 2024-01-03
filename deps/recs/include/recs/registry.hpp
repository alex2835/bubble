#pragma once
#include <unordered_map>
#include <string_view>
#include <string>
#include <tuple>
#include "utils.hpp"
#include "entity.hpp"
#include "pool.hpp"
#include "view.hpp"
#include "registry_meta.hpp"

namespace recs
{
class Registry
{
public:
    Registry() = default;
    // Not copyable and movable because entities refer to registry
    Registry( const Registry& ) = delete;
    Registry& operator=( const Registry& ) = delete;
    Registry( Registry&& ) = delete;
    Registry& operator=( Registry&& ) = delete;

    Entity CreateEntity();

    void RemoveEntity( Entity& entity );

    template <typename T, typename ...Args>
    void AddComponet( Entity entity, Args&& ...args );

    template <typename T>
    void RemoveComponet( Entity entity );

    template <typename T>
    T& GetComponent( Entity entity );

    template <typename ...Components>
    std::tuple<Components&...> GetComponents( Entity entity );

    template <typename T>
    bool HasComponent( Entity entity );

    template <typename ...Components>
    bool HasComponents( Entity entity );

    template <typename ...Components, typename F>
    void ForEach( F&& func );

    template <typename ...Components>
    View<Components...> GetView();

    void CloneInto( Registry& );

private:
    template <typename ...Components, typename F>
    void ForEachTuple( F&& func );

    template <typename ...Args, size_t Size, size_t ...Is>
    std::tuple<Args&...> MakeTupleFromPoolsAndIndicies( Pool* const( &pools )[Size], size_t const( &indicies )[Size], std::index_sequence<Is...> )
    {
        return std::forward_as_tuple( pools[Is]->template Get<Args>( indicies[Is] )... );
    }
    Pool& GetComponentPool( ComponentTypeID id );

    RegistryMeta mRegistryMeta;
    std::unordered_map<ComponentTypeID, Pool> mComponentPools;
};


// Implementation

template <typename T, typename ...Args>
void Registry::AddComponet( Entity entity, Args&& ...args )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Add component: invalid entity" );

    ComponentTypeID component_type = mRegistryMeta.GetComponentTypeID<T>();
    if ( !mRegistryMeta.HasComponentTypeID( component_type ) )
    {
        mRegistryMeta.AddComponentTypeID( component_type );
        mComponentPools.emplace( component_type, Pool::MakePool<T>() );
    }
    if ( entity.HasComponent<T>() )
        entity.GetComponent<T>() = T( std::forward<Args>( args )... );
    else
    {
        mRegistryMeta.AddComponent( entity, component_type );
        Pool& pool = mComponentPools[component_type];
        pool.Push<T>( entity, std::forward<Args>( args )... );
    }
}

template <typename T>
T& Registry::GetComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    ComponentTypeID component_type = mRegistryMeta.GetComponentTypeID<T>();
    if ( component_type )
    {
        Pool& pool = GetComponentPool( component_type );
        return pool.Get<T>( entity );
    }
    throw std::runtime_error( "Entity doesn't have such a component " );
}

template <typename T>
void Registry::RemoveComponet( Entity entity )
{
    ComponentTypeID component = mRegistryMeta.GetComponentTypeID<T>();
    if ( mRegistryMeta.HasComponent( entity, component ) )
    {
        auto iterator = std::find_if( mComponentPools.begin(), mComponentPools.end(),
                                      [=]( const auto& id_pool )
        {
            return id_pool.first == component;
        } );
        iterator->second.Remove( entity );
        mRegistryMeta.RemoveComponent( entity, component );
    }
}

template <typename T>
bool Registry::HasComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has component: invalid entity" );

    auto component = mRegistryMeta.GetComponentTypeID<T>();
    return mRegistryMeta.HasComponent( entity, component );
}

template <typename ...Components>
std::tuple<Components&...> Registry::GetComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    return std::forward_as_tuple( GetComponent<Components>( entity )... );
}

template <typename ...Components>
bool Registry::HasComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has components: invalid entity" );

    std::array<bool, sizeof...( Components )> has_components{
       mRegistryMeta.HasComponent( entity, mRegistryMeta.GetComponentTypeID<Components>() )... };
    for ( bool has_component : has_components )
        if ( !has_component )
            return false;
    return true;
}

template <typename ...Components>
View<Components...> Registry::GetView()
{
    std::vector<Entity> entities;
    std::vector<std::tuple<Components&...>> components;
    entities.reserve( mRegistryMeta.mEntityComponentsMeta.size() / 4 );
    components.reserve( mRegistryMeta.mEntityComponentsMeta.size() / 4 );

    ForEachTuple<Components...>( [&]( Entity entity, std::tuple<Components&...> comps )
    {
        entities.push_back( entity );
        components.push_back( comps );
    } );
    return View<Components...>( std::move( entities ), std::move( components ) );
}

template <typename ...Components, typename F>
void Registry::ForEach( F&& func )
{
    ForEachTuple<Components...>( [&func]( Entity entity, std::tuple<Components&...> components )
    {
        std::apply( std::forward<F>( func ), std::tuple_cat( std::make_tuple( entity ), components ) );
    } );
}

template <typename ...Components, typename F>
void Registry::ForEachTuple( F&& func )
{
    if ( !mRegistryMeta.HasComponentsTypeID<Components...>() )
        throw std::runtime_error( "No such components set" );

    const auto size = sizeof...( Components );
    Pool* pools[size];

    auto component_types = mRegistryMeta.GetComponentTypeIDs<Components...>();
    int i = 0;
    for ( auto component_type : component_types )
        pools[i++] = &GetComponentPool( component_type );

    size_t max_id = 0;
    size_t indicies[size] = { 0u };
    while ( true )
    {
        for ( int i = 0; i < size; i++ )
        {
            if ( indicies[i] >= pools[i]->Size() )
                return;

            if ( max_id < pools[i]->mEntities[indicies[i]].mID )
                max_id = pools[i]->mEntities[indicies[i]].mID;
        }

        bool next = false;
        for ( int i = 0; i < size; i++ )
        {
            while ( pools[i]->mEntities[indicies[i]].mID < max_id )
            {
                if ( pools[i]->mEntities[indicies[i]].mID > max_id )
                {
                    next = true;
                    break;
                }

                indicies[i]++;
                if ( indicies[i] >= pools[i]->Size() )
                    return;
            }
        }
        if ( next ) 
            continue;

        Entity entity( max_id, this );
        auto tuple = MakeTupleFromPoolsAndIndicies<Components...>( pools, indicies, std::make_index_sequence<size>{} );
        func( entity, tuple );

        for ( int i = 0; i < size; i++ )
            indicies[i]++;
    }
}

// Entity

template <typename T, typename ...Args>
Entity Entity::AddComponet( Args&& ...args )
{
    mRegistry->template AddComponet<T>( *this, std::forward<Args>( args )... );
    return *this;
}

template <typename T>
T& Entity::GetComponent()
{
    return mRegistry->template GetComponent<T>( *this );
}

template <typename T>
bool Entity::HasComponent()
{
    return mRegistry->template HasComponent<T>( *this );
}

template <typename ...Components>
bool Entity::HasComponents()
{
    return mRegistry->template HasComponents<Components...>( *this );
}

template <typename ...Components>
std::tuple<Components&...> Entity::GetComponents()
{
    return mRegistry->template GetComponents<Components...>( *this );
}

template <typename Component>
Entity Entity::RemoveComponet()
{
    mRegistry->RemoveComponet<Component>( *this );
    return *this;
}

}