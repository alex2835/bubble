
#include "recs/registry.hpp"
#include <cassert>

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity( mEntityCounter++, this );
    mEntitiesComponentTypeIds[entity];
    return entity;
}

Entity Registry::GetEntityById( size_t id )
{
    auto iter = mEntitiesComponentTypeIds.find( Entity( id, this ) );
    if ( iter == mEntitiesComponentTypeIds.end() )
        return INVALID_ENTITY;
    return iter->first;
}

void Registry::RemoveEntity( Entity& entity )
{
    auto components = GetEntityComponentsIds( entity );
    for ( auto component : components )
        mPools[component].Remove( entity );

    auto iter = mEntitiesComponentTypeIds.find( entity );
    assert( iter != mEntitiesComponentTypeIds.end() );
    mEntitiesComponentTypeIds.erase( iter );

    entity.mId = 0;
}

const std::unordered_map<std::string, ComponentTypeId, string_hash, std::equal_to<>>& Registry::AllComponentTypeIdsMap()
{
    return mComponents;
}

std::unordered_set<ComponentTypeId>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitiesComponentTypeIds[entity];
}

void Registry::EntityAddComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentTypeIds[entity];
    entityComponents.insert( componentId );
}

bool Registry::EntityHasComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentTypeIds[entity];
    return entityComponents.count( componentId );
}

void Registry::EntityRemoveComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentTypeIds[entity];
    auto iter = entityComponents.find( componentId );
    assert( iter != entityComponents.end() );
    entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentTypeId id )
{
    return mPools[id];
}

Pool& Registry::GetComponentPool( std::string_view component )
{
    const auto& componentsMap = AllComponentTypeIdsMap();
    auto iter = componentsMap.find( component );
    if ( iter == componentsMap.end() )
        throw std::runtime_error( std::format( "No such component: {}", component ) );
    return GetComponentPool( iter->second );
}

void Registry::UpdateEntityReferences()
{
    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> map;
    for ( auto entityComponents : mEntitiesComponentTypeIds )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = this;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    mEntitiesComponentTypeIds = std::move( map );
}

void Registry::CloneInto( Registry& registry )
{
    // clone 
    registry.mEntityCounter = mEntityCounter;
    registry.mComponentCounter = mComponentCounter;
    registry.mComponents = mComponents;
    //registry.mEntitiesComponentTypeIds = mEntitiesComponentTypeIds;
    //registry.mPools = mPools;

    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> map;
    for ( auto entityComponents : mEntitiesComponentTypeIds )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = &registry;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    registry.mEntitiesComponentTypeIds = std::move( map );
    
    for ( const auto& [componentId, pool] : registry.mPools )
    {
        registry.mPools[componentId] = pool.Clone();
        for ( auto& entity : registry.mPools[componentId].mEntities )
            entity.mRegistry = &registry;
    }
}

const std::unordered_set<recs::ComponentTypeId>& Registry::EntityComponentTypeIds( Entity entity )
{
    return mEntitiesComponentTypeIds[entity];
}

void Registry::EntityAddComponentId( Entity entity, ComponentTypeId componentId )
{
    auto iter = mPools.find( componentId );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Invalid componentId " + std::to_string( componentId ) );

    auto& pool = iter->second;
    pool.PushEmpty( entity );
    mEntitiesComponentTypeIds[entity].insert( componentId );
}

void Registry::EntityRemoveComponentId( Entity entity, ComponentTypeId componentId )
{
    auto iter = mPools.find( componentId );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Invalid componentId " + std::to_string( componentId ) );

    auto& pool = iter->second;
    pool.Remove( entity );
    mEntitiesComponentTypeIds[entity].erase( componentId );
}

recs::RuntimeView Registry::GetRuntimeView( const std::vector<std::string_view>& components )
{
    std::vector<Entity> entities;

    size_t minSize = std::numeric_limits<size_t>::max();
    for ( auto component : components )
        minSize = std::min( minSize, GetComponentPool( component ).Size() );
    entities.reserve( minSize );

    RuntimeForEach( components, [&]( Entity entity )
    {
        entities.push_back( entity );
    } );
    return RuntimeView( std::move( entities ) );
}

ComponentTypeId Registry::GetComponentTypeId( std::string_view name )
{
    auto iter = mComponents.find( name );
    if ( iter == mComponents.end() )
        return INVALID_COMPONENT_TYPE;
    return iter->second;
}



// Entity 
const std::unordered_set<ComponentTypeId>& Entity::EntityComponentTypeIds()
{
    return mRegistry->EntityComponentTypeIds( *this );
}

void Entity::EntityAddComponentId( ComponentTypeId componentId )
{
    return mRegistry->EntityAddComponentId( *this, componentId );
}

void Entity::EntityRemoveComponentId( ComponentTypeId componentId )
{
    return mRegistry->EntityRemoveComponentId( *this, componentId );
}


}