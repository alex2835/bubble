#pragma once
#include "engine/engine.hpp"
#include "scene_camera.hpp"
#include "editor_interface_loader.hpp"

namespace bubble
{
struct EditorState
{
    Window window;
    Framebuffer sceneViewport;
    SceneCamera sceneCamera;
    Entity selectedEntity;
};


class BubbleEditor
{
public:
    BubbleEditor();
    void Run();

    void EditTransform( float* cameraView,
                        float* cameraProjection,
                        float* matrix,
                        bool editTransformDecomposition );

private:
    EditorState mState;
    Engine mEngine;
    EditorInterfaceLoader mInterfaceLoader;
};

}
