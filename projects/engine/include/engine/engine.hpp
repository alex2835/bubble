#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/renderer.hpp"

namespace bubble
{
class Project;
class Scene;

struct Engine
{
    Timer mTimer;
    Camera mActiveCamera;
    Renderer mRenderer;
    
    // Attached project to run
    Project& mProject;

    Engine( Project& project );
    void OnStart();
    void OnEnd();
    void OnUpdate();
    void DrawScene( Framebuffer& framebuffer );
};

}
