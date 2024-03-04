#pragma once
#include "engine/serialization/scene_serialization.hpp"
#include "engine/utils/types.hpp"
#include <nlohmann/json.hpp>

namespace recs
{
using namespace nlohmann;
using namespace bubble;
void to_json( json& j, const Entity& entity )
{
    j = (size_t)entity;
}

void from_json( const json& j, Entity& entity )
{
    // We need to update entity refs after this
    auto entityId = size_t( j );
    entity = *(Entity*)&entityId;
}

void to_json( json& j, const Scene& scene )
{
    j["Entity counter"] = scene.mEntityCounter;
    j["Component counter"] = scene.mComponentCounter;
    j["Components"] = scene.mComponents;
    j["Entity components"] = scene.mEntitiesComponentIds;

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
            componentToJson( poolJson[pool.mEntities[i]], pool.GetRaw( i ) );
    }
}

void from_json( const json& j, Scene& scene )
{
    scene.mEntityCounter = j["Entity counter"];
    scene.mComponentCounter = j["Component counter"];
    scene.mComponents = j["Components"];
    scene.mEntitiesComponentIds = j["Entity components"];
    scene.UpdateEntityReferences();

    const json& poolsJson = j["Component pools"];
    for ( const string& componentName : poolsJson )
    {
        auto componentsIter = scene.mComponents.find( componentName );
        if ( componentsIter == scene.mComponents.end() )
            throw std::runtime_error( "Scene from_json failed. No such component: " + componentName );

        const auto componentId = componentsIter->second;
        auto poolsIter = scene.mPools.find( componentId );
        if ( poolsIter == scene.mPools.end() )
            throw std::runtime_error( "scene from_json failed. No such pool: " + componentName );

        Pool& pool = poolsIter->second;
        const json& poolJson = poolsJson[componentName];
        const auto& componentFromJson = ComponentManager::GetFromJson( componentName );

        for ( size_t entityId : poolJson )
            componentFromJson( poolJson[entityId], pool.PushEmpty( scene.GetEntityById( entityId ) ) );
    }
}

} // recs