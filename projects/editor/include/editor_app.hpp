#pragma once
#include "engine/engine.hpp"
#include "scene_camera.hpp"
#include "editor_interface_loader.hpp"

namespace bubble
{
struct EditorState
{
    Window mWindow;
    Framebuffer mSceneViewport;
    Framebuffer mObjectIdViewport;
    SceneCamera mSceneCamera;
    Entity mSelectedEntity;
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
