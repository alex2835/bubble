#pragma once
#include "engine/log/log.hpp"
#include "engine/window/window.hpp"
#include "engine/loader/loader.hpp"
#include "engine/renderer/renderer.hpp"
#include "engine/scene/scene.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"

namespace bubble
{
struct Engine
{
    Timer mTimer;
    Scene mScene;
    Renderer mRenderer;

    void OnUpdate();
    void DrawScene( const Camera& mCamera, const Framebuffer& mFramebuffer );
};

}
