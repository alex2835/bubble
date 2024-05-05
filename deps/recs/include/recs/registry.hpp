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
    Entity GetEntityById( size_t id );
    void RemoveEntity( Entity& entity );

    template <ComponentType Component>
    Registry& AddComponet();

    template <ComponentType Component, typename ...Args>
    Registry& AddComponet( Entity entity, Args&& ...args );

    template <ComponentType Component>
    Component& GetComponent( Entity entity );

    template <ComponentType ...Components>
    std::tuple<Components&...> GetComponents( Entity entity );

    template <ComponentType Component>
    bool HasComponent( Entity entity );

    template <ComponentType ...Components>
    bool HasComponents( Entity entity );

    template <ComponentType Component>
    void RemoveComponent( Entity entity );

    template <ComponentType ...Components, typename F>
    void ForEach( F&& func );

    template <typename F>
    void ForEachEntity( F&& func );

    template <ComponentType ...Components>
    View<Components...> GetView();

    template <typename F>
    void ForEachEntityComponentRaw( Entity entity, F&& func );

    void UpdateEntityReferences();
    void CloneInto( Registry& );

private:
    // Component ids management
    template <ComponentType Component>
    ComponentIdType AddComponentTypeId();

    template <ComponentType Component>
    ComponentIdType GetComponentTypeId();

    template <ComponentType ...Components>
    std::array<ComponentIdType, sizeof...(Components)> GetComponentsTypeId();

    template <ComponentType Component>
    bool HasComponentTypeId();

    template <ComponentType ...Components>
    bool HasComponentsTypeId();

    template <ComponentType Component>
    Component& EntityGetComponent( Entity entity, ComponentIdType component );

    void EntityAddComponent( Entity entity, ComponentIdType component );
    bool EntityHasComponent( Entity entity, ComponentIdType component );
    void EntityRemoveComponent( Entity entity, ComponentIdType component );
    std::unordered_set<ComponentIdType>& GetEntityComponentsIds( Entity entity );

    // For each
    template <ComponentType ...Components, typename F>
    void ForEachTuple( F&& func );

    template <typename ...Args, size_t Size, size_t ...Is>
    std::tuple<Args&...> MakeTupleFromPoolsAndIndicies( Pool* const( &pools )[Size], size_t const( &indicies )[Size], std::index_sequence<Is...> )
    {
        return std::forward_as_tuple( pools[Is]->template Get<Args>( indicies[Is] )... );
    }
    Pool& GetComponentPool( ComponentIdType id );

public:
    size_t mEntityCounter = 1;
    size_t mComponentCounter = 1;
    std::unordered_map<std::string, ComponentIdType, string_hash, std::equal_to<>> mComponents;
    std::unordered_map<Entity, std::unordered_set<ComponentIdType>> mEntitiesComponentIds;
    std::unordered_map<ComponentIdType, Pool> mPools;
};


// Component ids

template <ComponentType Component>
ComponentIdType Registry::GetComponentTypeId()
{
    auto iter = mComponents.find( Component::Name() );
    if ( iter == mComponents.end() )
        return INVALID_COMPONENT_TYPE;
    return iter->second;
}

template <ComponentType Component>
ComponentIdType Registry::AddComponentTypeId()
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

template <ComponentType ...Components>
std::array<ComponentIdType, sizeof...(Components)> Registry::GetComponentsTypeId()
{
    return { GetComponentTypeId<Components>()... };
}

template <ComponentType Component>
bool Registry::HasComponentTypeId()
{
    return mComponents.count( Component::Name() );
}

template <ComponentType ...Components>
bool Registry::HasComponentsTypeId()
{
    return ( HasComponentTypeId<Components>() && ... );
}

template <ComponentType Component>
Component& Registry::EntityGetComponent( Entity entity, ComponentIdType component )
{
    Pool& pool = GetComponentPool( component );
    return pool.Get<Component>( entity );
}


// Registry

template <ComponentType Component>
Registry& Registry::AddComponet()
{
    ComponentIdType componentType = GetComponentTypeId<Component>();
    if ( componentType == INVALID_COMPONENT_TYPE )
    {
        componentType = AddComponentTypeId<Component>();
        mPools.emplace( componentType, Pool::CreatePool<Component>() );
    }
    return *this;
}

template <ComponentType Component, typename ...Args>
Registry& Registry::AddComponet( Entity entity, Args&& ...args )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Add component: invalid entity" );

    ComponentIdType componentType = GetComponentTypeId<Component>();
    if ( componentType == INVALID_COMPONENT_TYPE )
    {
        componentType = AddComponentTypeId<Component>();
        mPools.emplace( componentType, Pool::CreatePool<Component>() );
    }
    if ( EntityHasComponent( entity, componentType ) )
    {
        auto& component = EntityGetComponent<Component>( entity, componentType );
        component = Component( std::forward<Args>( args )... );
    }
    else
    {
        EntityAddComponent( entity, componentType );
        Pool& pool = mPools[componentType];
        pool.Push<Component>( entity, std::forward<Args>( args )... );
    }
    return *this;
}

template <ComponentType Component>
Component& Registry::GetComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    ComponentIdType componentType = GetComponentTypeId<Component>();
    if ( componentType != INVALID_COMPONENT_TYPE )
    {
        Pool& pool = GetComponentPool( componentType );
        return pool.Get<Component>( entity );
    }
    throw std::runtime_error( "Entity doesn't have such a component " );
}

template <ComponentType ...Components>
std::tuple<Components&...> Registry::GetComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Get component: invalid entity" );

    return std::forward_as_tuple( GetComponent<Components>( entity )... );
}

template <ComponentType Component>
bool Registry::HasComponent( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has component: invalid entity" );

    auto componentId = GetComponentTypeId<Component>();
    if( componentId == INVALID_COMPONENT_TYPE )
        throw std::runtime_error( "HasComponent: invalid component type:" +
                                  std::string( Component::Name() ) );

    return EntityHasComponent( entity, componentId );
}

template <ComponentType ...Components>
bool Registry::HasComponents( Entity entity )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "Has components: invalid entity" );

    return ( EntityHasComponent( entity, GetComponentTypeId<Components>() ) && ... );
}


template <ComponentType Component>
void Registry::RemoveComponent( Entity entity )
{
    ComponentIdType component = GetComponentTypeId<Component>();
    if ( EntityHasComponent( entity, component ) )
    {
        Pool& pool = mPools[component];
        pool.Remove( entity );
        EntityRemoveComponent( entity, component );
    }
}


// For each

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

template <typename F>
void Registry::ForEachEntityComponentRaw( Entity entity, F&& func )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "ForEachEntityComponentRaw: Invalid entity" );

    const auto& components = mEntitiesComponentIds[entity];
    for ( const auto& [name, id] : mComponents )
    {
        if ( components.count( id ) )
        {
            Pool& pool = mPools[id];
            func( name, pool.GetRaw( entity ) );
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
    for ( const auto& [entity, _] : mEntitiesComponentIds )
        func( entity );
}

template <ComponentType ...Components, typename F>
void Registry::ForEachTuple( F&& func )
{
    if ( !HasComponentsTypeId<Components...>() )
        throw std::runtime_error( "No such components set: " +
              ( ( std::string( Components::Name() ) + ", " ) + ... ) );

    const auto size = sizeof...( Components );
    Pool* pools[size];

    auto componentTypes = GetComponentsTypeId<Components...>();
    int i = 0;
    for ( auto componentType : componentTypes )
        pools[i++] = &GetComponentPool( componentType );

    size_t max_id = 0;
    size_t indicies[size] = { 0u };
    while ( true )
    {
        for ( int i = 0; i < size; i++ )
        {
            if ( indicies[i] >= pools[i]->Size() )
                return;

            if ( max_id < pools[i]->mEntities[indicies[i]].mId )
                max_id = pools[i]->mEntities[indicies[i]].mId;
        }

        bool next = false;
        for ( int i = 0; i < size; i++ )
        {
            while ( pools[i]->mEntities[indicies[i]].mId < max_id )
            {
                if ( pools[i]->mEntities[indicies[i]].mId > max_id )
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