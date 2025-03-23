#include <nlohmann/json.hpp>
#include "engine/types/json.hpp"
#include "engine/types/array.hpp"
#include "engine/types/set.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/component_manager.hpp"

namespace bubble
{
using namespace nlohmann;

void Scene::ToJson( const Loader& loader, json& j )
{
    j["Entity counter"] = mEntityCounter;
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, componentTypeIds] : mEntitiesComponentTypeIds )
        entityComponentsJson[std::to_string( entity )] = componentTypeIds;

    json& poolsJson = j["Component pools"];
    for ( const auto& componentID : mComponents )
    {
        auto iter = mPools.find( componentID );
        if ( iter == mPools.end() )
            throw std::runtime_error( std::format( "No pool for component: {}", componentID ) );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];

        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        for ( size_t i = 0; i < pool.mEntities.size(); i++ )
        {
            auto entityStr = std::to_string( pool.mEntities[i] );
            componentToJson( loader, poolJson[entityStr], pool.GetRaw( i ) );
        }
    }
}

void Scene::FromJson( Loader& loader, const json& j )
{
    mEntityCounter = j["Entity counter"];
    // Entity components
    const json& entityComponentsJson = j["Entity components"];
    for ( const auto& [entityStr, componentsJson] : entityComponentsJson.items() )
    {
        hash_set<ComponentTypeId> components;
        for ( ComponentTypeId component : componentsJson )
            components.insert( component );

        u64 entityId = std::atoi( entityStr.c_str() );
        Entity entity = *(Entity*)&entityId;
        mEntitiesComponentTypeIds[entity] = components;
    }

    for ( const auto& [componentNameString, poolJson] : j["Component pools"].items() )
    {
        size_t componentID = ComponentManager::GetID( componentNameString );
        auto componentsIter = mComponents.find( componentID );
        if ( componentsIter == mComponents.end() )
            throw std::runtime_error( std::format( "Scene from_json failed. No such component: {}", componentID ) );

        auto poolsIter = mPools.find( componentID );
        if ( poolsIter == mPools.end() )
            throw std::runtime_error( std::format( "scene from_json failed. No such pool {}", componentID ) );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentID );

        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            u64 entityId = std::atoi( entityIdStr.c_str() );
            componentFromJson( loader, componentJson, pool.PushEmpty( GetEntityById( entityId ) ) );
        }
    }
}

} // recs