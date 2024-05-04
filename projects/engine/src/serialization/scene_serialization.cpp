#pragma once
#include "engine/serialization/scene_serialization.hpp"
#include "engine/utils/types.hpp"
#include <nlohmann/json.hpp>

namespace recs
{
using namespace nlohmann;
using namespace bubble;

void SceneToJson( const Loader& loader, json& j, const Scene& scene )
{
    j["Entity counter"] = scene.mEntityCounter;
    j["Component counter"] = scene.mComponentCounter;
    j["Components"] = scene.mComponents;
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, components] : scene.mEntitiesComponentIds )
        entityComponentsJson[std::to_string( entity )] = components;

    json& poolsJson = j["Component pools"];
    for ( const auto& [componentName, componentId] : scene.mComponents )
    {
        auto iter = scene.mPools.find( componentId );
        if ( iter == scene.mPools.end() )
            throw std::runtime_error( "No pool for component: " + componentName );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[componentName];
        const auto& componentToJson = ComponentManager::GetToJson( componentName );

        for ( size_t i = 0; i < pool.mEntities.size(); i++ )
        {
            auto entityStr = std::to_string( pool.mEntities[i] );
            componentToJson( loader, poolJson[entityStr], pool.GetRaw( i ) );
        }
    }
}

void SceneFromJson( Loader& loader, const json& j, Scene& scene )
{
    scene.mEntityCounter = j["Entity counter"];
    scene.mComponentCounter = j["Component counter"];
    scene.mComponents = j["Components"];
    // Entity components
    const json& entityCompnentsJson = j["Entity components"];
    for ( const auto& [entityStr, componetsJson] : entityCompnentsJson.items() )
    {
        hash_set<ComponentIdType> components;
        for ( ComponentIdType component : componetsJson )
            components.insert( component );

        u64 entityId = std::atoi( entityStr.c_str() );
        Entity entity = *(Entity*)&entityId;
        scene.mEntitiesComponentIds[entity] = components;
    }
    scene.UpdateEntityReferences();

    for ( const auto& [componentName, poolJson] : j["Component pools"].items() )
    {
        auto componentsIter = scene.mComponents.find( componentName );
        if ( componentsIter == scene.mComponents.end() )
            throw std::runtime_error( "Scene from_json failed. No such component: " + componentName );

        const auto componentId = componentsIter->second;
        auto poolsIter = scene.mPools.find( componentId );
        if ( poolsIter == scene.mPools.end() )
            throw std::runtime_error( "scene from_json failed. No such pool: " + componentName );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentName );

        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            u64 entityId = std::atoi( entityIdStr.c_str() );
            componentFromJson( loader, componentJson, pool.PushEmpty( scene.GetEntityById( entityId ) ) );
        }
    }
}

} // recs