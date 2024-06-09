
#include "recs/registry.hpp"
#include <cassert>

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity( mEntityCounter++, this );
    mEntitiesComponentIds[entity];
    return entity;
}

Entity Registry::GetEntityById( size_t id )
{
    auto iter = mEntitiesComponentIds.find( Entity( id, this ) );
    if ( iter == mEntitiesComponentIds.end() )
        return INVALID_ENTITY;
    return iter->first;
}

void Registry::RemoveEntity( Entity& entity )
{
    auto components = GetEntityComponentsIds( entity );
    for ( auto component : components )
        mPools[component].Remove( entity );

    auto iter = mEntitiesComponentIds.find( entity );
    assert( iter != mEntitiesComponentIds.end() );
    mEntitiesComponentIds.erase( iter );

    entity.mId = 0;
}

const std::unordered_map<std::string, ComponentTypeId, string_hash, std::equal_to<>>& Registry::AllComponentIdsMap()
{
    return mComponents;
}

std::unordered_set<ComponentTypeId>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitiesComponentIds[entity];
}

void Registry::EntityAddComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    entityComponents.insert( componentId );
}

bool Registry::EntityHasComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    return entityComponents.count( componentId );
}

void Registry::EntityRemoveComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    auto iter = entityComponents.find( componentId );
    assert( iter != entityComponents.end() );
    entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentTypeId id )
{
    return mPools[id];
}

void Registry::UpdateEntityReferences()
{
    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> map;
    for ( auto entityComponents : mEntitiesComponentIds )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = this;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    mEntitiesComponentIds = std::move( map );
}

void Registry::CloneInto( Registry& registry )
{
    // clone 
    registry.mEntityCounter = mEntityCounter;
    registry.mComponentCounter = mComponentCounter;
    registry.mComponents = mComponents;
    //registry.mEntitiesComponentIds = mEntitiesComponentIds;
    //registry.mPools = mPools;

    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> map;
    for ( auto entityComponents : mEntitiesComponentIds )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = &registry;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    registry.mEntitiesComponentIds = std::move( map );
    
    for ( const auto& [componentId, pool] : registry.mPools )
    {
        registry.mPools[componentId] = pool.Clone();
        for ( auto& entity : registry.mPools[componentId].mEntities )
            entity.mRegistry = &registry;
    }
}

const std::unordered_set<recs::ComponentTypeId>& Registry::EntityComponentIds( Entity entity )
{
    return mEntitiesComponentIds[entity];
}

void Registry::EntityAddComponentId( Entity entity, ComponentTypeId componentId )
{
    auto iter = mPools.find( componentId );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Invalid componentId " + std::to_string( componentId ) );

    auto& pool = iter->second;
    pool.PushEmpty( entity );
    mEntitiesComponentIds[entity].insert( componentId );
}

void Registry::EntityRemoveComponentId( Entity entity, ComponentTypeId componentId )
{
    auto iter = mPools.find( componentId );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Invalid componentId " + std::to_string( componentId ) );

    auto& pool = iter->second;
    pool.Remove( entity );
    mEntitiesComponentIds[entity].erase( componentId );
}


// Entity 
const std::unordered_set<ComponentTypeId>& Entity::EntityComponentIds()
{
    return mRegistry->EntityComponentIds( *this );
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