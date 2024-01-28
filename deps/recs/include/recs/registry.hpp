#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <string>
#include <tuple>
#include <ranges>
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
    // Not copyable and movable because entities refer to registry
    Registry( const Registry& ) = delete;
    Registry& operator=( const Registry& ) = delete;
    Registry( Registry&& ) = delete;
    Registry& operator=( Registry&& ) = delete;

    Entity CreateEntity();

    void RemoveEntity( Entity& entity );

    template <ComponentType Component, typename ...Args>
    void AddComponet( Entity entity, Args&& ...args );

    template <ComponentType Component>
    void RemoveComponent( Entity entity );

    template <ComponentType Component>
    Component& GetComponent( Entity entity );

    template <ComponentType ...Components>
    std::tuple<Components&...> GetComponents( Entity entity );

    template <ComponentType Component>
    bool HasComponent( Entity entity );

    template <ComponentType ...Components>
    bool HasComponents( Entity entity );

    template <ComponentType ...Components, typename F>
    void ForEach( F&& func );

    template <ComponentType ...Components>
    View<Components...> GetView();

    void CloneInto( Registry& );

private:
    // Ids
    template <ComponentType Component>
    ComponentTypeId AddComponentTypeId();

    template <ComponentType Component>
    ComponentTypeId GetComponentTypeId();

    template <ComponentType ...Component>
    std::array<ComponentTypeId, sizeof...(Component)> GetComponentsTypeId();

    template <ComponentType Component>
    bool HasComponentTypeId();

    template <ComponentType ...Component>
    bool HasComponentsTypeId();

    void EntityAddComponent( Entity entity, ComponentTypeId component );
    bool EntityHasComponent( Entity entity, ComponentTypeId component );
    void EntityRemoveComponent( Entity entity, ComponentTypeId component );
    std::unordered_set<ComponentTypeId>& GetEntityComponentsIds( Entity entity );

    // Ranges
    template <ComponentType ...Components, typename F>
    void ForEachTuple( F&& func );

    template <typename ...Args, size_t Size, size_t ...Is>
    std::tuple<Args&...> MakeTupleFromPoolsAndIndicies( Pool* const( &pools )[Size], size_t const( &indicies )[Size], std::index_sequence<Is...> )
    {
        return std::forward_as_tuple( pools[Is]->template Get<Args>( indicies[Is] )... );
    }
    Pool& GetComponentPool( ComponentTypeId id );

private:
    size_t mEntityCounter = 1;
    size_t mComponentCounter = 1;
    std::unordered_map<std::string, ComponentTypeId, string_hash, std::equal_to<>> mComponents;
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> mEntitysComponents;
    std::unordered_map<ComponentTypeId, Pool> mPools;
};



// Component ids

template <ComponentType Component>
ComponentTypeId Registry::GetComponentTypeId()
{
    auto iter = mComponents.find( Component::Name() );
    if ( iter == mComponents.end() )
        return INVALID_COMPONENT_TYPE;
    return iter->second;
}

template <ComponentType Component>
ComponentTypeId Registry::AddComponentTypeId()
{
    auto iter = mComponents.find( Component::Name() );
    if ( iter == mComponents.end() )
    {
        auto id = mComponentCounter++;
        mComponents[std::string( Component::Name() )] = id;
        return id;
    }
    return iter->second;
}

template <ComponentType ...Component>
std::array<ComponentTypeId, sizeof...(Component)> Registry::GetComponentsTypeId()
{
    return { GetComponentTypeId<Component>()... };
}

template <ComponentType Component>
bool Registry::HasComponentTypeId()
{
    return mComponents.count( Component::Name() );
}

template <ComponentType ...Component>
bool Registry::HasComponentsTypeId()
{
    return ( HasComponentTypeId<Component>() && ... );
}



// Registry

template <ComponentType Component, typename ...Args>
void Registry::AddComponet( Entity entity, Args&& ...args )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Add component: invalid entity" );

    ComponentTypeId component_type = GetComponentTypeId<Component>();
    if ( component_type == INVALID_COMPONENT_TYPE )
    {
        component_type = AddComponentTypeId<Component>();
        mPools.emplace( component_type, Pool::MakePool<Component>() );
    }
    if ( entity.HasComponent<Component>() )
        entity.GetComponent<Component>() = Component( std::forward<Args>( args )... );
    else
    {
        EntityAddComponent( entity, component_type );
        Pool& pool = mPools[component_type];
        pool.Push<Component>( entity, std::forward<Args>( args )... );
    }
}

template <ComponentType Component>
Component& Registry::GetComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    ComponentTypeId component_type = GetComponentTypeId<Component>();
    if ( component_type )
    {
        Pool& pool = GetComponentPool( component_type );
        return pool.Get<Component>( entity );
    }
    throw std::runtime_error( "Entity doesn't have such a component " );
}

template <ComponentType Component>
void Registry::RemoveComponent( Entity entity )
{
    ComponentTypeId component = GetComponentTypeId<Component>();
    if ( EntityHasComponent( entity, component ) )
    {
        Pool& pool = mPools[component];
        pool.Remove( entity );
        EntityRemoveComponent( entity, component );
    }
}

template <ComponentType Component>
bool Registry::HasComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has component: invalid entity" );

    auto componentId = GetComponentTypeId<Component>();
    auto& entityComponents = mEntitysComponents[entity];
    return entityComponents.count( componentId );
}

template <ComponentType ...Components>
std::tuple<Components&...> Registry::GetComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    return std::forward_as_tuple( GetComponent<Components>( entity )... );
}

template <ComponentType ...Components>
bool Registry::HasComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has components: invalid entity" );

    std::array<bool, sizeof...( Components )> has_components{
       HasComponentId( entity, GetComponentTypeId<Components>() )... };
    for ( bool has_component : has_components )
        if ( !has_component )
            return false;
    return true;
}

template <ComponentType ...Components>
View<Components...> Registry::GetView()
{
    std::vector<Entity> entities;
    std::vector<std::tuple<Components&...>> components;
    entities.reserve( 10 );
    components.reserve( 10 );

    ForEachTuple<Components...>( [&]( Entity entity, std::tuple<Components&...> comps )
    {
        entities.push_back( entity );
        components.push_back( comps );
    } );
    return View<Components...>( std::move( entities ), std::move( components ) );
}

template <ComponentType ...Components, typename F>
void Registry::ForEach( F&& func )
{
    ForEachTuple<Components...>( [&func]( Entity entity, std::tuple<Components&...> components )
    {
        std::apply( std::forward<F>( func ), std::tuple_cat( std::make_tuple( entity ), components ) );
    } );
}

template <ComponentType ...Components, typename F>
void Registry::ForEachTuple( F&& func )
{
    if ( !HasComponentsTypeId<Components...>() )
        throw std::runtime_error( "No such components set: " +
                                  ( ( std::string(Components::Name()) + ", " ) + ... ) );

    const auto size = sizeof...( Components );
    Pool* pools[size];

    auto component_types = GetComponentsTypeId<Components...>();
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

template <ComponentType Component, typename ...Args>
Entity Entity::AddComponet( Args&& ...args )
{
    mRegistry->template AddComponet<Component>( *this, std::forward<Args>( args )... );
    return *this;
}

template <ComponentType Component>
Component& Entity::GetComponent()
{
    return mRegistry->template GetComponent<Component>( *this );
}

template <ComponentType Component>
bool Entity::HasComponent()
{
    return mRegistry->template HasComponent<Component>( *this );
}

template <ComponentType ...Components>
bool Entity::HasComponents()
{
    return mRegistry->template HasComponents<Components...>( *this );
}

template <ComponentType ...Components>
std::tuple<Components&...> Entity::GetComponents()
{
    return mRegistry->template GetComponents<Components...>( *this );
}

template <ComponentType Component>
Entity Entity::RemoveComponent()
{
    mRegistry->RemoveComponent<Component>( *this );
    return *this;
}

}