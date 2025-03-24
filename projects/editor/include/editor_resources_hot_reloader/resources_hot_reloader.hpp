#pragma once
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include <thread>

namespace bubble
{
class Project;

class ProjectResourcesHotReloader
{
    struct UpdateInfo 
    {
        filesystem::file_time_type mFileLastUpdateTime;
        std::atomic<bool> mNeedUpdate = false;
    };

public:
    ProjectResourcesHotReloader( Project& project );
    ~ProjectResourcesHotReloader();
    void OnUpdate();

private:
    void StopThread();
    void CreateUpdater();

    Project& mProject;
    map<path, UpdateInfo> mUpdateInfoMap;
    std::thread mUpdater;
    bool mStop = true;
    
};

}
