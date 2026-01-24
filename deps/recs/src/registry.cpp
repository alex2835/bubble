
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

Entity Registry::CreateEntityWithId( size_t id )
{
    Entity entity( id );
    mEntitiesComponentTypeIds[entity];

    // Update counter if necessary to avoid ID collisions
    if ( id >= mEntityCounter )
        mEntityCounter = id + 1;

    return entity;
}

Entity Registry::GetEntityById( size_t id )
{
    auto iter = mEntitiesComponentTypeIds.find( Entity( id ) );
    if ( iter == mEntitiesComponentTypeIds.end() )
        return INVALID_ENTITY;
    return iter->first;
}

void Registry::RemoveEntity( Entity entity )
{
    auto components = GetEntityComponentsIds( entity );
    for ( auto component : components )
        mPools[component].Remove( entity );

    auto iter = mEntitiesComponentTypeIds.find( entity );
    assert( iter != mEntitiesComponentTypeIds.end() );
    mEntitiesComponentTypeIds.erase( iter );
}

Entity Registry::CopyEntity( Entity entity )
{
    auto newEntity = CreateEntity();
    auto& newEntityComponentIds = mEntitiesComponentTypeIds[newEntity];
    for ( auto componentId : GetEntityComponentsIds( entity ) )
    {
        newEntityComponentIds.insert( componentId );
        auto& pool = GetPool( componentId );
        void* newCompMem = pool.PushEmpty( newEntity );
        const void* compMem = pool.GetRaw( entity );
        pool.mDoCopy( compMem, newCompMem );
    }
    return newEntity;
}

Entity Registry::CopyEntityInto( Registry& targetRegistry, Entity entity )
{
    // Create entity in the target registry
    auto newEntity = targetRegistry.CreateEntity();
    auto& newEntityComponentIds = targetRegistry.mEntitiesComponentTypeIds[newEntity];

    // Copy all components from this registry to target registry
    for ( auto componentId : GetEntityComponentsIds( entity ) )
    {
        newEntityComponentIds.insert( componentId );

        // Get source pool from this registry
        auto& sourcePool = GetPool( componentId );

        // Get or create target pool in target registry
        auto& targetPool = targetRegistry.GetPool( componentId );

        // Allocate space in target pool and copy data
        void* newCompMem = targetPool.PushEmpty( newEntity );
        const void* compMem = sourcePool.GetRaw( entity );
        targetPool.mDoCopy( compMem, newCompMem );
    }
    return newEntity;
}

Entity Registry::CopyEntityIntoWithId( Registry& targetRegistry, Entity entity, size_t targetId )
{
    // Create entity in the target registry with specific ID
    auto newEntity = targetRegistry.CreateEntityWithId( targetId );
    auto& newEntityComponentIds = targetRegistry.mEntitiesComponentTypeIds[newEntity];

    // Copy all components from this registry to target registry
    for ( auto componentId : GetEntityComponentsIds( entity ) )
    {
        newEntityComponentIds.insert( componentId );

        // Get source pool from this registry
        auto& sourcePool = GetPool( componentId );

        // Get or create target pool in target registry
        auto& targetPool = targetRegistry.GetPool( componentId );

        // Allocate space in target pool and copy data
        void* newCompMem = targetPool.PushEmpty( newEntity );
        const void* compMem = sourcePool.GetRaw( entity );
        targetPool.mDoCopy( compMem, newCompMem );
    }
    return newEntity;
}

std::set<ComponentTypeId>& Registry::GetEntityComponentsIds( Entity entity )
{
    return mEntitiesComponentTypeIds[entity];
}

Pool& Registry::GetPool( ComponentTypeId componentId )
{
    auto iter = mPools.find( componentId );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Invalid componentId " + std::to_string( componentId ) );
    return iter->second;
}

void Registry::EntityAddComponent( Entity entity, ComponentTypeId componentId )
{
    auto& entityComponents = mEntitiesComponentTypeIds[entity];
    entityComponents.insert( componentId );
}

bool Registry::EntityHasComponent( Entity entity, ComponentTypeId componentId ) const
{
    auto iter = mEntitiesComponentTypeIds.find( entity );
    if ( iter == mEntitiesComponentTypeIds.end() )
        return false;
    const auto& entityComponents = iter->second;
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
    auto iter = mPools.find( id );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Registry::GetComponentPool failed due to invalid component id" );
    return iter->second;
}

const std::set<ComponentTypeId>& Registry::AllComponentTypeIds()
{
    return mComponents;
}

const std::set<ComponentTypeId>& Registry::EntityComponentTypeIds( Entity entity )
{
    return mEntitiesComponentTypeIds[entity];
}

void Registry::EntityAddComponentId( Entity entity, ComponentTypeId componentId )
{
    auto& pool = GetPool( componentId );
    pool.PushEmpty( entity );
    mEntitiesComponentTypeIds[entity].insert( componentId );
}

void Registry::EntityRemoveComponentId( Entity entity, ComponentTypeId componentId )
{
    auto& pool = GetPool( componentId );
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
//const std::set<ComponentTypeId>& Entity::EntityComponentTypeIds()
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