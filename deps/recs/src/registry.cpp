
#include "recs/registry.hpp"
#include <cassert>

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity( mEntityCounter++, this );
    mEntitysComponents[entity];
    return entity;
}

void Registry::RemoveEntity( Entity& entity )
{
    auto components = GetEntityComponentsIds( entity );
    for ( auto component : components )
        mPools[component].Remove( entity );

    auto iter = mEntitysComponents.find( entity );
    assert( iter != mEntitysComponents.end() );
    mEntitysComponents.erase( iter );

    entity.mID = 0;
}

std::unordered_set<ComponentTypeId>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitysComponents[entity];
}

void Registry::EntityAddComponent( Entity entity, ComponentTypeId component )
{
    auto& entityComponents = mEntitysComponents[entity];
    entityComponents.insert( component );
}

bool Registry::EntityHasComponent( Entity entity, ComponentTypeId component )
{
    auto& entityComponents = mEntitysComponents[entity];
    return entityComponents.count( component );
}

void Registry::EntityRemoveComponent( Entity entity, ComponentTypeId component )
{
    auto& entityComponents = mEntitysComponents[entity];
    auto iter = entityComponents.find( component );
    assert( iter != entityComponents.end() );
    entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentTypeId id )
{
    return mPools[id];
}

void Registry::CloneInto( Registry& registry )
{
    // clone 
    registry.mEntityCounter = mEntityCounter;
    registry.mComponentCounter = mComponentCounter;
    registry.mComponents = mComponents;
    //registry.mEntitysComponents = mEntitysComponents;
    //registry.mPools = mPools;

    // rewrite pointers
    std::unordered_map<Entity, std::unordered_set<ComponentTypeId>> map;
    for ( auto entityComponents : mEntitysComponents )
    {
        Entity entity = entityComponents.first;
        auto compoents = entityComponents.second;
        entity.mRegistry = &registry;
        map.emplace( std::make_pair( entity, std::move( compoents ) ) );
    }
    registry.mEntitysComponents = std::move( map );
    
    for ( const auto& [component, pool] : registry.mPools )
    {
        registry.mPools[component] = pool.Clone();
        for ( auto& entity : registry.mPools[component].mEntities )
            entity.mRegistry = &registry;
    }
}

}