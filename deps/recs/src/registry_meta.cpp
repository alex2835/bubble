
#include "recs/registry_meta.hpp"
#include "recs/registry.hpp"

namespace recs
{
int TYPE_ID_SEQUENCE = 1;

RegistryMeta::RegistryMeta()
    : mEntityCounter( 1 )
{
}

Entity RegistryMeta::CreateEntity( void* registry )
{
    Entity entity( mEntityCounter++, static_cast<Registry*>( registry ) );
    mEntityComponentsMeta[entity];
    return entity;
}

OptRef<std::vector<ComponentTypeID>> RegistryMeta::GetEntityComponents( Entity entity )
{
    auto iterator = std::find_if( mEntityComponentsMeta.begin(), mEntityComponentsMeta.end(),
                                  [entity]( const auto& entity_components )
    {
        return entity_components.first == entity;
    } );

    if ( iterator != mEntityComponentsMeta.end() )
        return iterator->second;

    return std::nullopt;
}

bool RegistryMeta::HasComponentTypeID( ComponentTypeID id )
{
    return std::find( mComponents.begin(), mComponents.end(), id ) != mComponents.end();
}

void RegistryMeta::AddComponentTypeID( ComponentTypeID id )
{
    mComponents.push_back( id );
}

bool RegistryMeta::HasComponent( Entity entity, ComponentTypeID component )
{
    auto components = GetEntityComponents( entity );
    return component && HasComponent( *components, component );
}

void RegistryMeta::AddComponent( Entity entity, ComponentTypeID component )
{
    mEntityComponentsMeta[entity].push_back( component );
}

bool RegistryMeta::HasComponent( Ref<std::vector<ComponentTypeID>> components, ComponentTypeID component )
{
    auto iterator = std::find( components.get().begin(), components.get().end(), component );
    return iterator != components.get().end();
}

void RegistryMeta::Remove( Entity entity )
{
    auto iterator = std::find_if( mEntityComponentsMeta.begin(), mEntityComponentsMeta.end(),
                                  [=]( const auto& entity_components )
    {
        return entity_components.first == entity;
    } );
    if ( iterator != mEntityComponentsMeta.end() )
        mEntityComponentsMeta.erase( iterator );
}

void RegistryMeta::RemoveComponent( Entity entity, ComponentTypeID component )
{
    auto iterator = std::find_if( mEntityComponentsMeta.begin(), mEntityComponentsMeta.end(),
                                  [=]( const auto& entity_components )
    {
        return entity_components.first == entity;
    } );

    if ( iterator != mEntityComponentsMeta.end() )
    {
        auto& components = iterator->second;
        auto component_iterator = std::find( components.begin(), components.end(), component );
        components.erase( component_iterator );
    }
}

}