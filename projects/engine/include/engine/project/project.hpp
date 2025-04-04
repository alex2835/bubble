#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/scripting/scripting_engine.hpp"

namespace bubble
{
class Project
{
    void LoadDefaultResources();
public:
    Project( WindowInput& input );
    void Create( const path& rootDir, const string& projectName );
    void Open( const path& filePath );
    void Save();

    string mName;
    path mRootFile;
    ScriptingEngine mScriptingEngine;
    Loader mLoader;
    Scene mScene;
    Scene mGameRunningScene;
};

}
