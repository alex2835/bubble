
#include "engine/utils/chrono.hpp"
#include "engine/project/project.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"

namespace bubble
{
bool IsTimeClose( filesystem::file_time_type start, filesystem::file_time_type end )
{
    return chrono::duration_cast<chrono::microseconds>( end - start ) < 1s;
}

filesystem::file_time_type ReadShaderLastFileTime( const path& shaderPath )
{
    filesystem::file_time_type lastFileTime = filesystem::file_time_type::min();
    for ( const path& file : filesystem::directory_iterator( shaderPath.parent_path() ) )
        if ( file.stem() == shaderPath.stem() )
            lastFileTime = std::max( lastFileTime, filesystem::last_write_time( file ) );
    return lastFileTime;
}



ProjectResourcesHotReloader::ProjectResourcesHotReloader( Project& project )
    : mProject( project )
{}

ProjectResourcesHotReloader::~ProjectResourcesHotReloader()
{
    StopThread();
}


void ProjectResourcesHotReloader::StopThread()
{
    mStop = true;
    if ( mUpdater.joinable() )
        mUpdater.join();
    mStop = false;
}


void ProjectResourcesHotReloader::CreateUpdater()
{
    StopThread();

    mUpdateInfoMap.clear();
    for ( const auto& [path, _] : mProject.mLoader.mShaders )
        mUpdateInfoMap[path].mFileLastUpdateTime = ReadShaderLastFileTime( path );

    mUpdater = std::thread( [&]()
    {
        while ( not mStop )
        {
            for ( auto& [shaderFile, info] : mUpdateInfoMap )
            {
                auto newLastTime = ReadShaderLastFileTime( shaderFile );
                if ( not IsTimeClose( info.mFileLastUpdateTime, newLastTime ) )
                {
                    info.mNeedUpdate = true;
                    info.mFileLastUpdateTime = newLastTime;
                }
            }
            std::this_thread::sleep_for( 1s );
        }
    } );
}


void ProjectResourcesHotReloader::OnUpdate()
{
    if ( mProject.mLoader.mShaders.size() != mUpdateInfoMap.size() )
        CreateUpdater();

    for ( auto& [path, info] : mUpdateInfoMap )
    {
        if ( info.mNeedUpdate )
        {
            LogInfo( "Reload shader: {}", path.string() );
            info.mNeedUpdate = false;
            try
            {
                auto newShader = LoadShader( path );
                mProject.mLoader.mShaders[path]->Swap( *newShader );
            }
            catch ( const std::exception& )
            {
                LogError( "Failed to reload shader" );
            }
        }
    }
}

}
