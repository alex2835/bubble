
#include "engine/project/project.hpp"
#include "engine/serialization/loader_serialization.hpp"
#include "engine/scene/component_manager.hpp"
#include "engine/types/set.hpp"
#include "nlohmann/json.hpp"
#include <fstream>

namespace bubble
{
constexpr string_view ROOT_FILE_EXT = ".bubble"sv;

void Project::LoadDefaultResources()
{
    mLoader.LoadShader( PHONG_SHADER );
    mLoader.LoadShader( WHITE_SHADER );
    mLoader.LoadShader( ONLY_DIFFUSE_SHADER );
}

Project::Project( WindowInput& input )
{
    mScriptingEngine.BindInput( input );
    mScriptingEngine.BindLoader( mLoader );
    mScriptingEngine.BindScene( mScene );
}

void Project::Create( const path& rootDir, const string& projectName )
{
    auto projectDir = rootDir / projectName;
    filesystem::create_directory( projectDir );

    mRootFile =  ( projectDir / projectName ).replace_extension( ROOT_FILE_EXT );
    if ( filesystem::exists( mRootFile ) )
        throw std::runtime_error( "Project with such name already exists: " + mRootFile.string() );

    LoadDefaultResources();
    Save();
}

void Project::Open( const path& rootFile )
{
    if ( !is_regular_file( rootFile ) || rootFile.extension() != ROOT_FILE_EXT )
        throw std::runtime_error( "Invalid project path: " + rootFile.string() );
    
    mRootFile = rootFile;
    mLoader.mProjectRootPath = rootFile.parent_path();

    std::ifstream stream( mRootFile );
    json projectJson = json::parse( stream );
    from_json( projectJson["Loader"], mLoader );
    LoadScene( projectJson["Scene"] );
}

void Project::Save()
{
    json projectJson;
    projectJson["Loader"] = mLoader;
    SaveScene( projectJson["Scene"] );

    std::ofstream projectFile( mRootFile );
    projectFile << projectJson.dump( 1 );
}


void Project::SaveScene( json& j )
{
    j["Entity counter"] = mScene.mEntityCounter;
    // Entity components
    auto& entityComponentsJson = j["Entity components"];
    for ( const auto& [entity, componentTypeIds] : mScene.mEntitiesComponentTypeIds )
        entityComponentsJson[std::to_string( entity )] = componentTypeIds;

    json& poolsJson = j["Component pools"];
    for ( const auto& componentID : mScene.mComponents )
    {
        auto iter = mScene.mPools.find( componentID );
        if ( iter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "No pool for component: {}", componentID ) );

        const auto& pool = iter->second;
        json& poolJson = poolsJson[ComponentManager::GetName( componentID )];

        const auto& componentToJson = ComponentManager::GetToJson( componentID );
        for ( size_t i = 0; i < pool.mEntities.size(); i++ )
        {
            auto entityStr = std::to_string( pool.mEntities[i] );
            componentToJson( poolJson[entityStr], *this, pool.GetRaw( i ) );
        }
    }
}

void Project::LoadScene( const json& j )
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
        size_t componentID = ComponentManager::GetID( componentNameString );
        auto componentsIter = mScene.mComponents.find( componentID );
        if ( componentsIter == mScene.mComponents.end() )
            throw std::runtime_error( std::format( "Scene from_json failed. No such component: {}", componentID ) );

        auto poolsIter = mScene.mPools.find( componentID );
        if ( poolsIter == mScene.mPools.end() )
            throw std::runtime_error( std::format( "scene from_json failed. No such pool {}", componentID ) );

        Pool& pool = poolsIter->second;
        const auto& componentFromJson = ComponentManager::GetFromJson( componentID );

        for ( const auto& [entityIdStr, componentJson] : poolJson.items() )
        {
            u64 entityId = std::atoi( entityIdStr.c_str() );
            componentFromJson( componentJson, *this, pool.PushEmpty( mScene.GetEntityById( entityId ) ) );
        }
    }
}

}
