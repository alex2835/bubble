#include "engine/pch/pch.hpp"
#include "engine/project/level.hpp"
#include "engine/project/project.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/scene/hierarchy.hpp"
#include "engine/project/prefab.hpp"
#include "engine/types/set.hpp"
#include "engine/scene/components/folder_component.hpp"
#include "engine/scene/components/hierarchy_component.hpp"
#include "engine/scene/components/tag_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace bubble
{
Level::Level()
{
    MakeRoot( "Level" );
}

void Level::Clear()
{
    // Scene first - see the header.
    mScene = Scene();
    MakeRoot( "Level" );
    mName.clear();
    mFile.clear();
}

Entity Level::MakeRoot( const string& name )
{
    const Entity root = mScene.CreateEntity();
    mScene.AddComponent<TagComponent>( root, name );
    mScene.AddComponent<TransformComponent>( root );
    mScene.AddComponent<FolderComponent>( root );
    mScene.AddComponent<HierarchyComponent>( root );
    mScene.SetRoot( root );
    return root;
}

void Level::SetRootName( const string& name )
{
    if ( mScene.HasEntity( Root() ) and mScene.HasComponent<TagComponent>( Root() ) )
        mScene.GetComponent<TagComponent>( Root() ).mName = name;
}

json Level::SaveScene( const Project& project ) const
{
    json j;
    j["Entity counter"] = mScene.mEntityCounter;
    j["Root"] = (u64)mScene.Root();
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, componentTypeIds] : mScene.mEntitiesComponentTypeIds )
        entityComponentsJson[std::to_string( entity )] = componentTypeIds;

    json& poolsJson = j["Component pools"];
    for ( const auto& componentID : mScene.mComponents )
    {
        const auto iter = mScene.mPools.find( componentID );
        if ( iter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "No pool for component: {}", componentID ) );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];

        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        const auto& poolEntities = pool.Entities();
        for ( size_t i = 0; i < poolEntities.size(); i++ )
        {
            const auto entityStr = std::to_string( poolEntities[i] );
            componentToJson( poolJson[entityStr], project, pool.GetRaw( i ) );
        }
    }
    return j;
}


void Level::LoadScene( const json& j, Project& project )
{
    mScene.mEntityCounter = j["Entity counter"];
    // Entity components
    const json& entityComponentsJson = j["Entity components"];
    for ( const auto& [entityStr, componentsJson] : entityComponentsJson.items() )
    {
        set<ComponentTypeId> components;
        for ( ComponentTypeId component : componentsJson )
            components.insert( component );

        u64 entityId = std::atoi( entityStr.c_str() );
        Entity entity = *(Entity*)&entityId;
        mScene.mEntitiesComponentTypeIds[entity] = components;
    }

    for ( const auto& [componentNameString, poolJson] : j["Component pools"].items() )
    {
        int componentID = ComponentManager::GetID( componentNameString );
        auto componentsIter = mScene.mComponents.find( componentID );
        if ( componentsIter == mScene.mComponents.end() )
            throw std::runtime_error( std::format( "Scene from_json failed. No such component: {}", componentID ) );

        auto poolsIter = mScene.mPools.find( componentID );
        if ( poolsIter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "scene from_json failed. No such pool {}", componentID ) );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentID );

        // json object keys are strings, so items() yields "1", "10", "100", "2"...
        // Feeding a sorted pool in that order makes every insert shift the tail
        // (O(n^2) load). Sort numerically first so each push appends instead.
        std::vector<std::pair<u64, const json*>> ordered;
        ordered.reserve( poolJson.size() );
        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
            ordered.emplace_back( std::strtoull( entityIdStr.c_str(), nullptr, 10 ), &componentJson );

        std::sort( ordered.begin(), ordered.end(),
                   []( const auto& a, const auto& b ) { return a.first < b.first; } );

        for ( const auto& [entityId, componentJson] : ordered )
            componentFromJson( *componentJson, project, pool.PushEmpty( mScene.GetEntityById( entityId ) ) );
    }
}


json Level::ToJson( const Project& project ) const
{
    json j;
    j["Scene"] = SaveScene( project );
    return j;
}

void Level::FromJson( const json& j, Project& project )
{
    // Loaded into an empty scene: the default root goes, the file brings one.
    mScene = Scene();
    LoadScene( j["Scene"], project );
    mScene.SetRoot( mScene.GetEntityById( j["Scene"].value( "Root", u64( 0 ) ) ) );
    if ( not mScene.HasEntity( Root() ) )
        throw std::runtime_error( "Level has no root entity" );
    // A file edited by hand may repeat a name among siblings; the second gets
    // a number, so every path leads somewhere.
    MakeNamesUnique( mScene, Root() );
    UpdateWorldTransforms( mScene );
}


void Level::Load( const path& absFile, Project& project )
{
    // A prefab is a level file under another extension.
    if ( not is_regular_file( absFile ) or ( absFile.extension() != LEVEL_FILE_EXT and absFile.extension() != PREFAB_FILE_EXT ) )
        throw std::runtime_error( "Invalid level path: " + absFile.string() );

    Clear();
    std::ifstream stream( absFile );
    FromJson( json::parse( stream ), project );
    mFile = absFile;
    mName = absFile.stem().string();
    // The root is labeled after the file, so the tree says which level it is
    // showing.
    SetRootName( mName );
    LogInfo( "{} opened: {}", absFile.extension() == PREFAB_FILE_EXT ? "Prefab" : "Level", absFile.string() );
}

void Level::Save( const path& absFile, const Project& project ) const
{
    std::ofstream levelFile( absFile );
    levelFile << ToJson( project ).dump( 1 );
    LogInfo( "{} saved: {}", absFile.extension() == PREFAB_FILE_EXT ? "Prefab" : "Level", absFile.string() );
}

}
