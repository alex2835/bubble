#include "engine/pch/pch.hpp"
#include "engine/utils/chrono.hpp"
#include "engine/project/project.hpp"
#include "utils/resources_hot_reloader.hpp"

namespace bubble
{
namespace
{
constexpr auto POLL_INTERVAL = 1s;

// How long a file must have been quiet before it is worth reloading. An editor
// writing a large shader can be caught mid-save, and a truncated file would be
// reported as a compile error rather than retried.
constexpr auto FILE_SETTLE_TIME = 200ms;


// A file counts as changed when its timestamp differs at all - not when it is
// newer by some margin. Reverting to an older revision moves the timestamp
// backwards and still has to reload, and two saves a few hundred milliseconds
// apart are two changes.
//
// The settle delay is measured against the wall clock, never against the
// previously seen timestamp: a file still being written is skipped on this tick
// and picked up on the next, instead of being dropped for good.
bool FileChanged( filesystem::file_time_type known, filesystem::file_time_type current )
{
    if ( current == known )
        return false;

    // A timestamp in the future - clock skew on a network share - is settled.
    const auto age = filesystem::file_time_type::clock::now() - current;
    return age <= decltype( age )::zero() or age >= FILE_SETTLE_TIME;
}


// Every file making up one shader: the stages share a stem and sit next to each
// other. The error_code overload matters - a shader whose directory has been
// deleted while the editor is open must not throw out of the render loop.
vector<path> ShaderStageFiles( const path& shaderPath )
{
    vector<path> paths;
    std::error_code error;
    for ( const auto& entry : filesystem::directory_iterator( shaderPath.parent_path(), error ) )
    {
        if ( entry.path().stem() == shaderPath.stem() )
            paths.push_back( entry.path() );
    }
    return paths;
}


// The newest timestamp across a resource's files. A file that cannot be read
// contributes nothing; if none of them can, the result is min(), which reads as
// a change and lets the reload report the real problem.
filesystem::file_time_type NewestWriteTime( const vector<path>& files )
{
    auto newest = filesystem::file_time_type::min();
    for ( const auto& file : files )
    {
        std::error_code error;
        const auto writeTime = filesystem::last_write_time( file, error );
        if ( not error )
            newest = std::max( newest, writeTime );
    }
    return newest;
}


// The file behind a resource is gone. Drop it from the loader and null it out
// wherever the scene still points at it.
template <class ComponentType, class ResourceType, class MemberPtr>
void DropResource( Project& project,
                   hash_map<path, Ref<ResourceType>>& resources,
                   const path& loaderPath,
                   MemberPtr member )
{
    const Ref<ResourceType> removed = resources[loaderPath];
    resources.erase( loaderPath );

    project.mScene.ForEach<ComponentType>( [&]( Entity, ComponentType& component )
    {
        if ( component.*member and ( component.*member )->mName == removed->mName )
            component.*member = nullptr;
    } );
}

} // namespace


ProjectResourcesHotReloader::ProjectResourcesHotReloader( Project& project, UIGlobals& uiGlobals )
    : mProject( project ),
      mUIGlobals( uiGlobals )
{
    mWatcherThread = std::thread( [this]{ WatchLoop(); } );
}


ProjectResourcesHotReloader::~ProjectResourcesHotReloader()
{
    {
        std::lock_guard lock( mStopMutex );
        mStop = true;
    }
    mStopCondition.notify_all();

    if ( mWatcherThread.joinable() )
        mWatcherThread.join();
}


// ----------------------------------------------------------------- watcher --

void ProjectResourcesHotReloader::WatchLoop()
{
    while ( not mStop )
    {
        ScanForChanges();

        std::unique_lock lock( mStopMutex );
        mStopCondition.wait_for( lock, POLL_INTERVAL, [this]{ return mStop.load(); } );
    }
}


void ProjectResourcesHotReloader::ScanForChanges()
{
    // Copy out what to stat, so the filesystem work - the slow part, and
    // unbounded on a network drive - happens with no lock held.
    struct Scan
    {
        path mLoaderPath;
        ResourceType mType;
        vector<path> mFiles;
        filesystem::file_time_type mWriteTime;
    };

    vector<Scan> scans;
    {
        std::lock_guard lock( mWatchMutex );
        scans.reserve( mWatchList.size() );
        for ( const auto& [loaderPath, watched] : mWatchList )
            scans.push_back( Scan{ .mLoaderPath = loaderPath,
                                   .mType = watched.mType,
                                   .mFiles = watched.mFiles } );
    }

    for ( auto& scan : scans )
        scan.mWriteTime = NewestWriteTime( scan.mFiles );

    std::lock_guard lock( mWatchMutex );
    for ( const auto& scan : scans )
    {
        // The render thread may have rebuilt the watch list while this scan ran.
        const auto iter = mWatchList.find( scan.mLoaderPath );
        if ( iter == mWatchList.end() )
            continue;

        WatchedResource& watched = iter->second;
        if ( not FileChanged( watched.mLastWriteTime, scan.mWriteTime ) )
            continue;

        // Recorded here rather than after the reload: a failed reload should be
        // reported once and then wait for the next save, not retried every tick.
        watched.mLastWriteTime = scan.mWriteTime;
        mPendingReloads.insert( PendingReload{ .mLoaderPath = scan.mLoaderPath,
                                               .mType = scan.mType } );
    }
}


// ------------------------------------------------------------------ render --

void ProjectResourcesHotReloader::OnUpdate()
{
    if ( mForceWatchListRebuild or IsWatchListStale() )
        RebuildWatchList();

    ReloadPending();
}


bool ProjectResourcesHotReloader::IsWatchListStale() const
{
    // Two integer comparisons, whatever the project size. This runs every frame
    // on the render thread and has to stay free.
    //
    // It cannot move to the watcher thread: it reads the loader's resource
    // maps, and those belong to the render thread - LoadShader compiles GL
    // objects, and opening a project replaces the Loader wholesale. Reading
    // them from another thread would race with an emplace rehashing the table.
    if ( mProject.mLoader.mResourcesGeneration != mSeenResourcesGeneration )
        return true;

    // A resource can also leave without the loader noticing: ReloadShader and
    // ReloadScript erase from those maps directly when a file has gone. That
    // always moves the total, so the size covers it.
    std::lock_guard lock( mWatchMutex );
    return mProject.mLoader.mShaders.size() + mProject.mLoader.mScripts.size()
           != mWatchList.size();
}


void ProjectResourcesHotReloader::RebuildWatchList()
{
    mForceWatchListRebuild = false;
    mSeenResourcesGeneration = mProject.mLoader.mResourcesGeneration;

    std::lock_guard lock( mWatchMutex );

    mWatchList.clear();

    auto watch = [&]( const auto& resources, ResourceType type )
    {
        for ( const auto& [loaderPath, _] : resources )
        {
            const path absPath = mProject.mLoader.RelAbsFromProjectPath( loaderPath ).abs;
            auto files = type == ResourceType::Shader
                         ? ShaderStageFiles( absPath )
                         : vector<path>{ absPath };

            mWatchList.emplace( loaderPath,
                                WatchedResource{ .mType = type,
                                                 .mFiles = files,
                                                 .mLastWriteTime = NewestWriteTime( files ) } );
        }
    };
    watch( mProject.mLoader.mShaders, ResourceType::Shader );
    watch( mProject.mLoader.mScripts, ResourceType::Script );

    // Keep anything already queued that this project still has. A rebuild is
    // triggered by some *other* resource appearing, and the fresh timestamps
    // above would otherwise absorb a save that arrived moments earlier - the
    // file would look unchanged from here on and never reload.
    std::erase_if( mPendingReloads, [this]( const PendingReload& reload )
    {
        return not mWatchList.contains( reload.mLoaderPath );
    } );
}


void ProjectResourcesHotReloader::ReloadPending()
{
    set<PendingReload> pending;
    {
        std::lock_guard lock( mWatchMutex );
        pending.swap( mPendingReloads );
    }

    for ( const auto& reload : pending )
    {
        try
        {
            switch ( reload.mType )
            {
                case ResourceType::Shader: ReloadShader( reload.mLoaderPath ); break;
                case ResourceType::Script: ReloadScript( reload.mLoaderPath ); break;
            }
        }
        catch ( const std::exception& e )
        {
            LogError( "Failed to reload {}: {}", reload.mLoaderPath.string(), e.what() );
        }
    }
}


void ProjectResourcesHotReloader::ReloadShader( const path& loaderPath )
{
    // The resource may have been dropped between the scan queueing it and now.
    const auto existing = mProject.mLoader.mShaders.find( loaderPath );
    if ( existing == mProject.mLoader.mShaders.end() )
        return;

    LogInfo( "Reload shader: {}", loaderPath.string() );

    const path absPath = mProject.mLoader.RelAbsFromProjectPath( loaderPath ).abs;
    if ( const auto newShader = LoadShader( absPath ) )
    {
        existing->second->Swap( *newShader );

        // The reloaded shader may have gained or lost uniforms, and every
        // component using it still holds a table keyed by the old set. Without
        // this, a uniform added to a shader never appears in the inspector
        // until the project is reopened, and a removed one never leaves.
        set<string> missing;
        set<string> retyped;
        mProject.mScene.ForEach<ShaderComponent>(
        [&]( Entity, ShaderComponent& shaderComponent )
        {
            if ( shaderComponent.mShader != existing->second )
                return;
            const auto dropped = shaderComponent.RebuildUniforms( mProject.mScriptingEngine );
            missing.insert( dropped.mMissing.begin(), dropped.mMissing.end() );
            retyped.insert( dropped.mRetyped.begin(), dropped.mRetyped.end() );
        } );
        LogDroppedShaderUniforms( existing->second,
                                  DroppedUniforms{ .mMissing = { missing.begin(), missing.end() },
                                                   .mRetyped = { retyped.begin(), retyped.end() } } );
        return;
    }

    DropResource<ShaderComponent>( mProject, mProject.mLoader.mShaders,
                                   loaderPath, &ShaderComponent::mShader );
    mForceWatchListRebuild = true;
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;
}


void ProjectResourcesHotReloader::ReloadScript( const path& loaderPath )
{
    const auto existing = mProject.mLoader.mScripts.find( loaderPath );
    if ( existing == mProject.mLoader.mScripts.end() )
        return;

    LogInfo( "Reload script: {}", loaderPath.string() );

    const path absPath = mProject.mLoader.RelAbsFromProjectPath( loaderPath ).abs;
    if ( const auto newScript = LoadScript( absPath ) )
    {
        existing->second->Swap( *newScript );
        return;
    }

    DropResource<ScriptComponent>( mProject, mProject.mLoader.mScripts,
                                   loaderPath, &ScriptComponent::mScript );
    mForceWatchListRebuild = true;
    mUIGlobals.mNeedUpdateProjectFilesWindow = true;
}

}
