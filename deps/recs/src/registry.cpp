
#include "recs/registry.hpp"
#include <cassert>
//#include <print>

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity( mEntityCounter++ );
    //std::println("created entity: {}, counter {}, adress {}", entity.mId, mEntityCounter, (uint64_t)this );
    mEntitiesComponentTypeIds[entity];
    return entity;
}

Entity Registry::GetEntityById( size_t id )
{
    auto iter = mEntitiesComponentTypeIds.find( Entity( id ) );
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

const std::unordered_set<recs::ComponentTypeId>& Registry::AllComponentTypeIds()
{
    return mComponents;
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

//std::vector<Entity> Registry::GetRuntimeView( const std::vector<std::string_view>& components )
//{
//    std::vector<Entity> entities;
//
//    size_t minSize = std::numeric_limits<size_t>::max();
//    for ( auto component : components )
//        minSize = std::min( minSize, GetComponentPool( component ).Size() );
//    entities.reserve( minSize );
//
//    RuntimeForEach( components, [&]( Entity entity )
//    {
//        entities.push_back( entity );
//    } );
//    return entities;
//}

//ComponentTypeId Registry::GetComponentTypeId( std::string_view name )
//{
//    auto iter = mComponents.find( name );
//    if ( iter == mComponents.end() )
//        return INVALID_COMPONENT_TYPE;
//    return iter->second;
//}



// Entity 
//const std::unordered_set<ComponentTypeId>& Entity::EntityComponentTypeIds()
//{
//    return mRegistry->EntityComponentTypeIds( *this );
//}
//
//void Entity::EntityAddComponentId( ComponentTypeId componentId )
//{
//    return mRegistry->EntityAddComponentId( *this, componentId );
//}
//
//void Entity::EntityRemoveComponentId( ComponentTypeId componentId )
//{
//    return mRegistry->EntityRemoveComponentId( *this, componentId );
//}


}