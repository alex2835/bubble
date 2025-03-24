
#include "engine/project/project.hpp"
#include "engine/serialization/loader_serialization.hpp"
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
    mScriptingEngine.bindInput( input );
    mScriptingEngine.bindLoader( mLoader );
    mScriptingEngine.bindScene( mGameRunningScene );
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
    mScene.FromJson( mLoader, projectJson["Scene"] );
}

void Project::Save()
{
    json projectJson;
    projectJson["Loader"] = mLoader;
    mScene.ToJson( mLoader, projectJson["Scene"] );

    std::ofstream projectFile( mRootFile );
    projectFile << projectJson.dump( 1 );
}

}
