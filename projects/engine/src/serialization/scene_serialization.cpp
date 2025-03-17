#include <nlohmann/json.hpp>
#include "engine/types/json.hpp"
#include "engine/types/array.hpp"
#include "engine/types/set.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/components_manager.hpp"

namespace bubble
{
using namespace nlohmann;

void Scene::ToJson( const Loader& loader, json& j )
{
    j["Entity counter"] = mEntityCounter;
    j["Component counter"] = mComponentCounter;
    j["Components"] = mComponents;
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, componentTypeIds] : mEntitiesComponentTypeIds )
        entityComponentsJson[std::to_string( entity )] = componentTypeIds;

    json& poolsJson = j["Component pools"];
    for ( const auto& [componentName, componentId] : mComponents )
    {
        auto iter = mPools.find( componentId );
        if ( iter == mPools.end() )
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

void Scene::FromJson( Loader& loader, const json& j )
{
    mEntityCounter = j["Entity counter"];
    //mComponentCounter = j["Component counter"];
    //mComponents = j["Components"];
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
    UpdateEntityReferences();

    for ( const auto& [componentName, poolJson] : j["Component pools"].items() )
    {
        auto componentsIter = mComponents.find( componentName );
        if ( componentsIter == mComponents.end() )
            throw std::runtime_error( "Scene from_json failed. No such component: " + componentName );

        const auto componentId = componentsIter->second;
        auto poolsIter = mPools.find( componentId );
        if ( poolsIter == mPools.end() )
            throw std::runtime_error( "scene from_json failed. No such pool: " + componentName );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentName );

        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            u64 entityId = std::atoi( entityIdStr.c_str() );
            componentFromJson( loader, componentJson, pool.PushEmpty( GetEntityById( entityId ) ) );
        }
    }
}

} // recs