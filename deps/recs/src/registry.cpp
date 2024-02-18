
#include "recs/registry.hpp"
#include <cassert>

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity( mEntityCounter++, this );
    mEntitysComponentIds[entity];
    return entity;
}

Entity Registry::GetEntityById( size_t id )
{
    auto iter = mEntitysComponentIds.find( Entity( id, this ) );
    if ( iter == mEntitysComponentIds.end() )
        return INVALID_ENTITY;
    return iter->first;
}

void Registry::RemoveEntity( Entity& entity )
{
    auto components = GetEntityComponentsIds( entity );
    for ( auto component : components )
        mPools[component].Remove( entity );

    auto iter = mEntitysComponentIds.find( entity );
    assert( iter != mEntitysComponentIds.end() );
    mEntitysComponentIds.erase( iter );

    entity.mID = 0;
}

std::unordered_set<ComponentIdType>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitysComponentIds[entity];
}

void Registry::EntityAddComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitysComponentIds[entity];
    entityComponents.insert( componentId );
}

bool Registry::EntityHasComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitysComponentIds[entity];
    return entityComponents.count( componentId );
}

void Registry::EntityRemoveComponent( Entity entity, ComponentIdType componentId )
{
    auto& entityComponents = mEntitysComponentIds[entity];
    auto iter = entityComponents.find( componentId );
    assert( iter != entityComponents.end() );
    entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentIdType id )
{
    return mPools[id];
}

void Registry::CloneInto( Registry& registry )
{
    // clone 
    registry.mEntityCounter = mEntityCounter;
    registry.mComponentCounter = mComponentCounter;
    registry.mComponents = mComponents;
    //registry.mEntitysComponentIds = mEntitysComponentIds;
    //registry.mPools = mPools;

    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentIdType>> map;
    for ( auto entityComponents : mEntitysComponentIds )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = &registry;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    registry.mEntitysComponentIds = std::move( map );
    
    for ( const auto& [componentId, pool] : registry.mPools )
    {
        registry.mPools[componentId] = pool.Clone();
        for ( auto& entity : registry.mPools[componentId].mEntities )
            entity.mRegistry = &registry;
    }
}

}