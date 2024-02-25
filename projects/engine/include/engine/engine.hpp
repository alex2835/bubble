#pragma once
#include "engine/utils/types.hpp"
#include "engine/log/log.hpp"

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
    ComponentManager mComponentManager;
    Scene mRunningScene;
    Renderer mRenderer;

    void OnUpdate();
    void DrawScene( const Camera& mCamera, const Framebuffer& mFramebuffer );
};
}
