#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT Project
{
public:
    const string& Name();
    const path& Root();
    Scene& Scene();

    void Open( const path& rootPath );
    void Save();

private:
    string mName;
    path mRoot;
    bubble::Scene mScene;
    Loader mLoader;
};

}
