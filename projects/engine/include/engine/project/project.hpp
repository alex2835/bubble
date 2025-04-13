#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/loader/loader.hpp"

namespace bubble
{
class Project
{
    void LoadDefaultResources();
    void SaveScene( json& j );
    void LoadScene( const json& j );
public:
    Project( WindowInput& input );
    void Create( const path& rootDir, const string& projectName );
    void Open( const path& filePath );
    void Save();

    string mName;
    path mRootFile;
    ScriptingEngine mScriptingEngine;
    PhysicsEngine mPhysicsEngine;
    Loader mLoader;
    Scene mScene;
};

}
