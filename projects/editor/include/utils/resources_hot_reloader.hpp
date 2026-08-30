#pragma once
#include "engine/types/map.hpp"
#include "engine/types/set.hpp"
#include "engine/loader/loader.hpp"
#include "utils/ui_globals.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace bubble
{
class Project;

// Watches the project's shader and script files and reloads them in place when
// they change on disk.
//
// Two threads and one mutex:
//
//   watcher   stats the watched files once a second and, for each resource
//             whose timestamp moved, adds an entry to mPendingReloads.
//   render    drains mPendingReloads in OnUpdate and performs the reloads,
//             which need the GL context, and rebuilds the watch list whenever
//             the project's resources change.
//
// mWatchMutex guards mWatchList and mPendingReloads. Nothing else is shared, so
// a frame with nothing to reload costs one uncontended lock and an empty swap.
class ProjectResourcesHotReloader
{
    enum class ResourceType
    {
        Shader,
        Script
    };

    // One entry of the watch list. A shader is several files - its stages share
    // a stem - and a change to any of them reloads the whole thing.
    struct WatchedResource
    {
        ResourceType mType = ResourceType::Shader;
        vector<path> mFiles;
        filesystem::file_time_type mLastWriteTime;
    };

    // A resource whose files changed, handed from the watcher to the render
    // thread. Held in a set, so a resource saved twice before the render thread
    // gets to it is still reloaded once.
    struct PendingReload
    {
        path mLoaderPath;
        ResourceType mType = ResourceType::Shader;

        bool operator<( const PendingReload& other ) const
        {
            if ( mLoaderPath != other.mLoaderPath )
                return mLoaderPath < other.mLoaderPath;
            return mType < other.mType;
        }
    };

public:
    ProjectResourcesHotReloader( Project& project, UIGlobals& uiGlobals );
    ~ProjectResourcesHotReloader();

    ProjectResourcesHotReloader( const ProjectResourcesHotReloader& ) = delete;
    ProjectResourcesHotReloader& operator=( const ProjectResourcesHotReloader& ) = delete;

    // Render thread, once a frame.
    void OnUpdate();

private:
    // Render thread.
    bool IsWatchListStale() const;
    void RebuildWatchList();
    void ReloadPending();
    void ReloadShader( const path& loaderPath );
    void ReloadScript( const path& loaderPath );

    // Watcher thread.
    void WatchLoop();
    void ScanForChanges();

    Project& mProject;
    UIGlobals& mUIGlobals;

    // The loader generation the watch list was built from. Generations start at
    // 1, so the first frame always rebuilds.
    u64 mSeenResourcesGeneration = 0;
    bool mForceWatchListRebuild = false;

    mutable std::mutex mWatchMutex;
    map<path, WatchedResource> mWatchList;
    set<PendingReload> mPendingReloads;

    // Shutdown. mStop is atomic so the watcher's loop condition is not a data
    // race on its own; the condition variable lets the destructor interrupt the
    // sleep instead of waiting out a full interval.
    std::mutex mStopMutex;
    std::condition_variable mStopCondition;
    std::atomic<bool> mStop = false;
    std::thread mWatcherThread;
};

}
