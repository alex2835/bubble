#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT Project
{
public:
    void Create( const path& rootDir, const string& projectName );
    void Open( const path& filePath );
    void Save();

    string mName;
    path mRootFile;
    Scene mScene;
    Loader mLoader;
};

}
