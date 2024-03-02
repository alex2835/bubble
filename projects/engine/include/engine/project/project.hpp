#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT Project
{
public:
    void Open( const path& rootPath );
    void Save();

    string mName;
    path mRoot;
    Scene mScene;
    Loader mLoader;
};

}
