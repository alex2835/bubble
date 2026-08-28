
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
    if ( id == (size_t)INVALID_ENTITY )
        throw std::runtime_error( "CreateEntityWithId: id 0 is reserved for INVALID_ENTITY" );

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
    // Look up first: operator[] would insert an entry for an unknown entity and
    // then happily "remove" it, hiding the caller's mistake.
    auto iter = mEntitiesComponentTypeIds.find( entity );
    assert( iter != mEntitiesComponentTypeIds.end() );
    if ( iter == mEntitiesComponentTypeIds.end() )
        return;

    for ( auto component : iter->second )
        GetPool( component ).Remove( entity );

    mEntitiesComponentTypeIds.erase( iter );
}

void Registry::RemoveEntities( std::span<const Entity> entities )
{
    if ( entities.empty() )
        return;

    // Pool::Remove needs a sorted list and ignores ids it does not hold, so one
    // sorted list serves every pool.
    std::vector<Entity> sorted;
    sorted.reserve( entities.size() );
    for ( Entity entity : entities )
    {
        if ( mEntitiesComponentTypeIds.contains( entity ) )
            sorted.push_back( entity );
    }

    std::sort( sorted.begin(), sorted.end(),
               []( Entity a, Entity b ) { return (size_t)a < (size_t)b; } );
    sorted.erase( std::unique( sorted.begin(), sorted.end(),
                               []( Entity a, Entity b ) { return (size_t)a == (size_t)b; } ),
                  sorted.end() );

    if ( sorted.empty() )
        return;

    for ( const ComponentTypeId componentId : mComponents )
        GetPool( componentId ).Remove( std::span<const Entity>( sorted ) );

    for ( Entity entity : sorted )
        mEntitiesComponentTypeIds.erase( entity );
}

Entity Registry::CopyEntity( Entity entity )
{
    auto newEntity = CreateEntity();
    auto& newEntityComponentIds = mEntitiesComponentTypeIds[newEntity];
    for ( auto componentId : GetEntityComponentsIds( entity ) )
    {
        newEntityComponentIds.insert( componentId );
        auto& pool = GetPool( componentId );

        // PushEmpty can reallocate and shift the pool's storage, so the source
        // address is only valid once the new slot exists. Never cache it before.
        void* newCompMem = pool.PushEmpty( newEntity );
        const void* compMem = pool.GetRaw( entity );
        pool.mDoDelete( newCompMem );
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

        // Source and target pools. The target registry must already have the
        // component type registered - GetPool throws otherwise.
        auto& sourcePool = GetPool( componentId );
        auto& targetPool = targetRegistry.GetPool( componentId );

        // PushEmpty may reallocate and shift storage. When &targetRegistry == this
        // that is the same pool as sourcePool, so read the source only afterwards.
        void* newCompMem = targetPool.PushEmpty( newEntity );
        const void* compMem = sourcePool.GetRaw( entity );
        targetPool.mDoDelete( newCompMem );
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

        // Source and target pools. The target registry must already have the
        // component type registered - GetPool throws otherwise.
        auto& sourcePool = GetPool( componentId );
        auto& targetPool = targetRegistry.GetPool( componentId );

        // PushEmpty may reallocate and shift storage. When &targetRegistry == this
        // that is the same pool as sourcePool, so read the source only afterwards.
        void* newCompMem = targetPool.PushEmpty( newEntity );
        const void* compMem = sourcePool.GetRaw( entity );
        targetPool.mDoDelete( newCompMem );
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
    auto entityIter = mEntitiesComponentTypeIds.find( entity );
    assert( entityIter != mEntitiesComponentTypeIds.end() );
    if ( entityIter == mEntitiesComponentTypeIds.end() )
        return;

    auto& entityComponents = entityIter->second;
    auto iter = entityComponents.find( componentId );
    assert( iter != entityComponents.end() );
    if ( iter != entityComponents.end() )
        entityComponents.erase( iter );
}

Pool& Registry::GetComponentPool( ComponentTypeId id )
{
    return const_cast<Pool&>( std::as_const( *this ).GetComponentPool( id ) );
}

const Pool& Registry::GetComponentPool( ComponentTypeId id ) const
{
    auto iter = mPools.find( id );
    if ( iter == mPools.end() )
        throw std::runtime_error( "Registry::GetComponentPool failed due to invalid component id" );
    return iter->second;
}

const std::set<ComponentTypeId>& Registry::AllComponentTypeIds() const
{
    return mComponents;
}

const std::set<ComponentTypeId>& Registry::EntityComponentTypeIds( Entity entity ) const
{
    auto iter = mEntitiesComponentTypeIds.find( entity );
    if ( iter == mEntitiesComponentTypeIds.end() )
        throw std::runtime_error( "Registry::EntityComponentTypeIds: entity not found" );
    return iter->second;
}

void Registry::EntityAddComponentId( Entity entity, ComponentTypeId componentId )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "EntityAddComponentId: Invalid entity" );

    auto& pool = GetPool( componentId );
    if ( EntityHasComponent( entity, componentId ) )
        return;

    pool.PushEmpty( entity );
    mEntitiesComponentTypeIds[entity].insert( componentId );
}

void Registry::EntityRemoveComponentId( Entity entity, ComponentTypeId componentId )
{
    if ( entity == INVALID_ENTITY )
        throw std::runtime_error( "EntityRemoveComponentId: Invalid entity" );

    auto& pool = GetPool( componentId );
    if ( !EntityHasComponent( entity, componentId ) )
        return;

    pool.Remove( entity );
    mEntitiesComponentTypeIds[entity].erase( componentId );
}

}