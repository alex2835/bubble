
#include "engine/project/project.hpp"
#include "engine/serialization/loader_serialization.hpp"
#include "nlohmann/json.hpp"
#include <fstream>

namespace bubble
{
constexpr string_view ROOT_FILE_EXT = ".bubble"sv;

Project::Project()
{
    // Order is strict
    mScene.AddComponet<TagComponent>()
          .AddComponet<ModelComponent>()
          .AddComponet<TransformComponent>();
}

void Project::Create( const path& rootDir, const string& projectName )
{
    auto projectDir = rootDir / projectName;
    filesystem::create_directory( projectDir );

    mRootFile =  ( projectDir / projectName ).replace_extension( ROOT_FILE_EXT );
    if ( filesystem::exists( mRootFile ) )
        throw std::runtime_error( "Project with such name already exists: " + mRootFile.string() );
    Save();
}

void Project::Open( const path& rootFile )
{
    if ( !is_regular_file( rootFile ) || rootFile.extension() != ROOT_FILE_EXT )
        throw std::runtime_error( "Invalid project path: " + rootFile.string() );
    
    mRootFile = rootFile;
    std::ifstream stream( mRootFile );
    json projectJson = json::parse( stream );
    mLoader = projectJson["Loader"];
    Scene::FromJson( mScene, mLoader, projectJson["Scene"] );
}

void Project::Save()
{
    json projectJson;
    projectJson["Loader"] = mLoader;
    Scene::ToJson( mScene, mLoader, projectJson["Scene"] );

    std::ofstream projectFile( mRootFile );
    projectFile << projectJson.dump( 1 );
}

}
