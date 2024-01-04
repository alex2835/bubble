#pragma once
#include "window/window.hpp"
#include "loader/loader.hpp"
#include "renderer/renderer.hpp"
#include "scene/scene.hpp"
#include "project/project.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT Engine
{
    Timer mTimer;
    Project mProject;
    Loader mLoader;
    Scene mScene;
    Renderer mRenderer;

    void OnUpdate();
};
}
