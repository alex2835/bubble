
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

std::unordered_set<ComponentIdType>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitiesComponentIds[entity];
}

void Registry::EntityAddComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    entityComponents.insert( componentId );
}

bool Registry::EntityHasComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    return entityComponents.count( componentId );
}

void Registry::EntityRemoveComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitiesComponentIds[entity];
    auto iter = entityComponents.find( componentId );
    assert( iter != entityComponents.end() );
    entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentIdType id )
{
    return mPools[id];
}

void Registry::UpdateEntityReferences()
{
    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentIdType>> map;
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
    std::unordered_map<Entity, std::unordered_set<ComponentIdType>> map;
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

}