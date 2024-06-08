#pragma once
#include "engine/utils/types.hpp"
#include "engine/utils/chrono.hpp"
#include "engine/loader/loader.hpp"
#include <thread>

namespace bubble
{
class ShaderHotReloader
{
    struct UpdateInfo 
    {
        filesystem::file_time_type mFileLastUpdateTime;
        std::atomic<bool> mNeedUpdate = false;
    };

public:
    ShaderHotReloader( Loader& loader );
    ~ShaderHotReloader();
    void OnUpdate();

private:
    void StopThread();
    void CreateUpdater();

    Loader& mLoader;
    map<path, UpdateInfo> mUpdateInfoMap;
    std::thread mUpdater;
    bool mStop = true;
    
};

}
