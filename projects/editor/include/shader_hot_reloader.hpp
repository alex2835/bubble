#pragma once
#include "engine/utils/types.hpp"
#include "engine/utils/chrono.hpp"
#include "engine/loader/loader.hpp"

namespace bubble
{
class ShaderHotReloader
{
    struct UpdateInfo 
    {
        filesystem::file_time_type mFileLastUpdateTime;
        bool mNeedUpdate = false;
        bool mStop = false;
    };
public:
    ShaderHotReloader()
    {

    }

    void CreateUpdater()
    {
        mStop = false;
        mUpdater.join();

        mUpdater = std::thread( [&, updateInfoMap = mUpdateInfoMap]()
        {
            while ( not mStop )
            {
                for ( const auto& [shaderFile, info] : updateInfoMap )
                {

                }
                std::this_thread::sleep_for( 1000ms );
            }
        } );

    }

    void OnUpdate()
    {
        if ( mLoader.mShaders.size() != mUpdateInfoMap.size() )
            CreateUpdater();
    }


private:
    Loader mLoader;
    map<path, UpdateInfo> mUpdateInfoMap;
    std::thread mUpdater;
    bool mStop = true;
    
};

}
