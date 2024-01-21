#pragma once
#include "engine/engine.hpp"
#include "scene_camera.hpp"
#include "editor_interface_loader.hpp"

namespace bubble
{
class BubbleEditor
{
public:
    BubbleEditor();
    void Run();
private:
    Window mWindow;
    Framebuffer mSceneViewport;
    SceneCamera mSceneCamera;
    Engine mEngine;
    EditorInterfaceLoader mInterfaceLoader;
};

}
