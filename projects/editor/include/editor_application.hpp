#pragma once
#include "engine/engine.hpp"
#include "scene_camera.hpp"
#include "editor_state.hpp"
#include "editor_interface_loader.hpp"

namespace bubble
{
class BubbleEditor : EditorState
{
    enum class EditorMode
    {
        Editing,
        Runing
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

    Engine mEngine;
    EditorInterfaceLoader mInterfaceLoader;
};

}
