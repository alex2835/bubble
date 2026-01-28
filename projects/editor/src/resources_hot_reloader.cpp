#include "engine/pch/pch.hpp"
#include "engine/utils/chrono.hpp"
#include "engine/project/project.hpp"
#include "utils/resources_hot_reloader.hpp"

namespace bubble
{
bool IsTimeClose( filesystem::file_time_type start, filesystem::file_time_type end )
{
    return chrono::duration_cast<chrono::microseconds>( end - start ) < 1s;
}

vector<path> GetShaderFiles( const path& shaderPath )
{
    vector<path> paths;
    for ( const path& file : filesystem::directory_iterator( shaderPath.parent_path() ) )
    {
        if ( file.stem() == shaderPath.stem() )
            paths.push_back( file );
    }
    return paths;
}

filesystem::file_time_type ReadFilesLastTime( const vector<path>& resoucePaths )
{
    filesystem::file_time_type lastFileTime = filesystem::file_time_type::min();
    std::error_code error;
    for ( const auto& resoucePath : resoucePaths )
        lastFileTime = std::max( lastFileTime, filesystem::last_write_time( resoucePath, error ) );
    return lastFileTime;
}



ProjectResourcesHotReloader::ProjectResourcesHotReloader( Project& project, UIGlobals& uiGlobals )
    : mProject( project ),
      mUIGlobals( uiGlobals )
{
    mFileUpdateChecker = std::thread( [&]()
    {
        while ( not mStop )
        {
            mMapMutex.lock();
            for ( auto& [_, info] : mFilesToUpdateMap )
            {
                auto newLastTime = ReadFilesLastTime( info.mFiles );
                if ( not IsTimeClose( info.mFileLastUpdateTime, newLastTime ) )
                {
                    info.mNeedUpdate = true;
                    info.mFileLastUpdateTime = newLastTime;
                }
            }
            mMapMutex.unlock();
            std::this_thread::sleep_for( 1s );
        }
    } );
}

ProjectResourcesHotReloader::~ProjectResourcesHotReloader()
{
    mStop = true;
    if ( mFileUpdateChecker.joinable() )
        mFileUpdateChecker.join();
}


void ProjectResourcesHotReloader::OnUpdate()
{
    // Update map
    const bool newFiles = mProject.mLoader.mShaders.size() + mProject.mLoader.mScripts.size() != mFilesToUpdateMap.size();
    const bool needUpdate = newFiles or mForceFilesUpdate;
    mForceFilesUpdate = false;

    if ( needUpdate )
    {
        std::lock_guard<std::mutex> lock( mMapMutex );

        mFilesToUpdateMap.clear();
        auto filler = [&]( const auto& resources, ResourceType type ) {
            for ( const auto& [loaderPath, _] : resources )
            {
                auto [relPath, absPath] = mProject.mLoader.RelAbsFromProjectPath( loaderPath );
                auto files = type == ResourceType::Shader
                             ? GetShaderFiles( absPath )
                             : vector<path>{ absPath };

                mFilesToUpdateMap.emplace( loaderPath, ResourceReloadInfo{
                    .mType = type,
                    .mFiles = files,
                    .mFileLastUpdateTime = ReadFilesLastTime( files ),
                    .mNeedUpdate = false
                } );
            }
        };
        filler( mProject.mLoader.mShaders, ResourceType::Shader );
        filler( mProject.mLoader.mScripts, ResourceType::Script );

    }
        
    // Check is files were updated
    for ( auto& [loaderPath, info] : mFilesToUpdateMap )
    {
        if ( info.mNeedUpdate )
        {
            switch ( info.mType )
            {
                case ResourceType::Shader:
                {
                    LogInfo( "Reload shader: {}", loaderPath.string() );
                    info.mNeedUpdate = false;
                    try
                    {
                        auto [_, absPath] = mProject.mLoader.RelAbsFromProjectPath( loaderPath );
                        const auto newShader = LoadShader( absPath );
                        if ( newShader )
                            mProject.mLoader.mShaders[loaderPath]->Swap( *newShader );
                        else
                        {
                            auto shaderToRemove = mProject.mLoader.mShaders[loaderPath];
                            mProject.mLoader.mShaders.erase( loaderPath );
                            mProject.mScene.ForEach<ShaderComponent>( [&]( Entity _, ShaderComponent& shaderComp ) {
                                if ( shaderComp.mShader and shaderComp.mShader->mName == shaderToRemove->mName )
                                    shaderComp.mShader = nullptr;
                            } );
                            mForceFilesUpdate = true;
                            mUIGlobals.mNeedUpdateProjectFilesWindow = true;
                        }
                    }
                    catch ( const std::exception& e )
                    {
                        LogError( "Failed to reload shader: {}", e.what() );
                    }
                } break;

                case ResourceType::Script:
                {
                    LogInfo( "Reload script: {}", loaderPath.string() );
                    info.mNeedUpdate = false;
                    try
                    {
                        auto [_, absPath] = mProject.mLoader.RelAbsFromProjectPath( loaderPath );
                        const auto newScript = LoadScript( absPath );
                        if ( newScript )
                            mProject.mLoader.mScripts[loaderPath]->Swap( *newScript );
                        else
                        {
                            auto scriptToRemove = mProject.mLoader.mScripts[loaderPath];
                            mProject.mLoader.mScripts.erase( loaderPath );
                            mProject.mScene.ForEach<ScriptComponent>( [&]( Entity _, ScriptComponent& scriptComp ) {
                                if ( scriptComp.mScript and scriptComp.mScript->mName == scriptToRemove->mName )
                                    scriptComp.mScript = nullptr;
                            } );
                            mForceFilesUpdate = true;
                            mUIGlobals.mNeedUpdateProjectFilesWindow = true;
                        }
                    }
                    catch ( const std::exception& e )
                    {
                        LogError( "Failed to reload script: {}", e.what() );
                    }
                } break;
            }
        }
    }
}

}
