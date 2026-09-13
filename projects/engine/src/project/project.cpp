#include "engine/pch/pch.hpp"
#include "engine/project/project.hpp"
#include "engine/serialization/loader_serialization.hpp"
#include "engine/serialization/any_serialization.hpp"
#include "engine/types/set.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <fstream>

namespace bubble
{
constexpr string_view ROOT_FILE_EXT = ".bubble"sv;
constexpr string_view DEFAULT_LEVEL_NAME = "main"sv;

void Project::LoadDefaultResources()
{
    mLoader.LoadShader( PHONG_SHADER );
    mLoader.LoadShader( WHITE_SHADER );
    mLoader.LoadShader( ONLY_DIFFUSE_SHADER );
}

Project::Project()
    : mGlobalState( CreateScope<Any>( mScriptingEngine.CreateTable() ) )
{
}

Project::~Project()
{
}

void Project::Create( const path& rootDir, const string& projectName )
{
    auto projectDir = rootDir / projectName;
    filesystem::create_directory( projectDir );

    mRootFile = ( projectDir / projectName ).replace_extension( ROOT_FILE_EXT );
    if ( filesystem::exists( mRootFile ) )
        throw std::runtime_error( "Project with such name already exists: " + mRootFile.string() );

    mName = projectName;
    mLoader.mProjectRootDir = projectDir;
    LoadDefaultResources();
    NewLevel( string( DEFAULT_LEVEL_NAME ) );
    mStartupLevel = CurrentLevel();
    Save();
}


void Project::Save() const
{
    BUBBLE_ASSERT( IsValid(), "Try to save invalid project" );
    SaveTo( mRootFile, mLevel.mFile );
}

void Project::SaveTo( const path& projectFile, const path& levelFile ) const
{
    json projectJson;
    projectJson["Loader"] = mLoader;
    projectJson["GlobalState"] = SaveAnyValue( *mGlobalState );
    projectJson["StartupLevel"] = mStartupLevel.generic_string();

    std::ofstream stream( projectFile );
    stream << projectJson.dump( 1 );
    LogInfo( "Project saved: {}", projectFile.string() );

    if ( mLevel.IsValid() )
        mLevel.Save( levelFile, *this );
}


void Project::Open( const path& rootFile, bool openStartupLevel )
{
    if ( !is_regular_file( rootFile ) || rootFile.extension() != ROOT_FILE_EXT )
        throw std::runtime_error( "Invalid project path: " + rootFile.string() );

    mRootFile = rootFile;
    mName = rootFile.stem().string();
    mLoader.mProjectRootDir = rootFile.parent_path();

    // Before anything loads: the shaders listed in the project file are
    // compiled during from_json below, and they have to see the project's own
    // modules while they are.
    SetProjectShaderModulesDir( mLoader.mProjectRootDir / PROJECT_SHADER_MODULES_SUBDIR );

    std::ifstream stream( mRootFile );
    json projectJson = json::parse( stream );
    from_json( projectJson["Loader"], mLoader );
    mGlobalState = CreateScope<Any>( LoadAnyValue( mScriptingEngine, projectJson["GlobalState"] ) );
    LogInfo( "Project opened: {}", mRootFile.string() );

    if ( projectJson.contains( "Scene" ) )
    {
        // Pre-levels layout: the scene lives in the project file. Split it out.
        mLevel.Clear();
        mLevel.FromJson( projectJson, *this );
        mLevel.mName = DEFAULT_LEVEL_NAME;
        mLevel.mFile = LevelsDir() / mLevel.mName;
        mLevel.mFile.replace_extension( LEVEL_FILE_EXT );
        mLevel.mTreeRoot->mState = mLevel.mName;
        mStartupLevel = CurrentLevel();

        filesystem::create_directories( LevelsDir() );
        Save();
        LogInfo( "Project migrated: scene moved to {}", mLevel.mFile.string() );
        return;
    }

    mStartupLevel = path( string( projectJson.value( "StartupLevel", "" ) ) );
    if ( mStartupLevel.empty() )
    {
        // No startup level recorded (or the file is hand made) - take the
        // first one there is, or make one so the project is usable.
        const auto levels = Levels();
        if ( levels.empty() )
            NewLevel( string( DEFAULT_LEVEL_NAME ) );
        else
            OpenLevel( levels.front() );
        mStartupLevel = CurrentLevel();
        return;
    }
    if ( openStartupLevel )
        OpenLevel( mStartupLevel );
}


bool Project::IsValid() const
{
    return not mRootFile.empty();
}


vector<path> Project::Levels() const
{
    vector<path> levels;
    const path levelsDir = LevelsDir();
    if ( not filesystem::is_directory( levelsDir ) )
        return levels;

    for ( const auto& entry : filesystem::recursive_directory_iterator( levelsDir ) )
    {
        if ( entry.is_regular_file() and entry.path().extension() == LEVEL_FILE_EXT )
            levels.push_back( filesystem::relative( entry.path(), RootDir() ) );
    }
    std::ranges::sort( levels );
    return levels;
}

void Project::NewLevel( const string& name )
{
    if ( name.empty() )
        throw std::runtime_error( "A level needs a name" );

    path file = LevelsDir() / name;
    file.replace_extension( LEVEL_FILE_EXT );
    if ( filesystem::exists( file ) )
        throw std::runtime_error( "Level already exists: " + file.string() );

    filesystem::create_directories( file.parent_path() );
    mLevel.Clear();
    mLevel.mName = name;
    mLevel.mFile = file;
    mLevel.mTreeRoot->mState = name;
    mLevel.Save( file, *this );
}

void Project::OpenLevel( const path& relFile )
{
    mLevel.Load( RootDir() / relFile, *this );
}

path Project::CurrentLevel() const
{
    if ( not mLevel.IsValid() )
        return {};
    return filesystem::relative( mLevel.mFile, RootDir() );
}

}
