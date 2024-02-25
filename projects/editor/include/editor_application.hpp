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
    enum class EditorMode
    {
        Edit,
        Run
    };

public:
    BubbleEditor();
    void Run();

    void SetUniformBuffer();
    void DrawProjectScene();
    void DrawSceneObjectId();
private:
    EditorMode mEditorMode;
    Ref<Shader> mObjectIdShader;

    EditorState mState;
    Engine mEngine;
    EditorInterfaceLoader mInterfaceLoader;
};

}
