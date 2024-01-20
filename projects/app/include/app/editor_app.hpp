#pragma once
#include "engine/engine.hpp"
#include "engine/serialization/event_serialization.hpp"
#include "scene_camera.hpp"
#include "interface/editor_interfaces.hpp"

namespace bubble
{
class BubbleEditor
{
public:
    BubbleEditor();
    void Run();
private:
    Window mWindow;
    //Framebuffer mSceneViewport;
    SceneCamera mSceneCamera;
    Engine mEngine;
    EditorInterfaces mInterfaces;
};

}
