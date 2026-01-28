#pragma once
#include "engine/types/map.hpp"
#include "engine/loader/loader.hpp"
#include "utils/ui_globals.hpp"
#include <thread>

namespace bubble
{
class Project;

class ProjectResourcesHotReloader
{
    enum class ResourceType
    {
        Shader,
        Script
    };

    struct ResourceReloadInfo
    {
        ResourceType mType = ResourceType::Shader;
        vector<path> mFiles
            ;
        filesystem::file_time_type mFileLastUpdateTime;
        bool mNeedUpdate = false;
    };

public:
    ProjectResourcesHotReloader( Project& project, UIGlobals& uiGlobals );
    ~ProjectResourcesHotReloader();
    void OnUpdate();

private:
    Project& mProject;
    UIGlobals& mUIGlobals;
    bool mForceFilesUpdate = false;
    
    std::mutex mMapMutex;
    map<path, ResourceReloadInfo> mFilesToUpdateMap;
    std::thread mFileUpdateChecker;
    bool mStop = false;
    
};

}
