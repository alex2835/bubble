
#include "recs/registry.hpp"

namespace recs
{
Entity Registry::CreateEntity()
{
    Entity entity = mRegistryMeta.CreateEntity( this );
    return entity;
}

void Registry::RemoveEntity( Entity& entity )
{
    auto components = mRegistryMeta.GetEntityComponents( entity );
    if ( components )
    {
        for ( auto component_id : components->get() )
        {
            auto iterator = std::find_if( mComponentPools.begin(), mComponentPools.end(),
                                          [=]( const auto& id_pool )
            {
                return id_pool.first == component_id;
            } );
            if ( iterator != mComponentPools.end() )
                iterator->second.Remove( entity );
        }
        mRegistryMeta.Remove( entity );
    }
    entity.mID = 0;
}

Pool& Registry::GetComponentPool( ComponentTypeID id )
{
    auto iterator = std::find_if( mComponentPools.begin(), mComponentPools.end(),
                                  [id]( const auto& id_pool )
    {
        return id_pool.first == id;
    } );
    return iterator->second;
}

void Registry::CloneInto( Registry& registry )
{
    // clone 
    registry.mRegistryMeta = mRegistryMeta;

    for ( const auto& [id, pool] : mComponentPools )
        registry.mComponentPools.emplace( std::make_pair( id, pool.Clone() ) );

    // rewrite pointers
    std::unordered_map<Entity, std::vector<ComponentTypeID>> map;
    for ( std::pair<Entity, std::vector<ComponentTypeID>> entity_components : registry.mRegistryMeta.mEntityComponentsMeta )
    {
        entity_components.first.mRegistry = &registry;
        map.emplace( std::move( entity_components ) );
    }
    registry.mRegistryMeta.mEntityComponentsMeta = std::move( map );

    for ( auto& [type_id, pool] : registry.mComponentPools )
        for ( auto& entity : pool.mEntities )
            entity.mRegistry = &registry;
}

}