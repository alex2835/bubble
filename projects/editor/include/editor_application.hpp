#pragma once
#include "engine/engine.hpp"
#include "scene_camera.hpp"
#include "editor_state.hpp"
#include "editor_interface_hot_reloader.hpp"
#include "resource_hot_reloader.hpp"

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
    void DrawProjectScene();
private:
    EditorMode mEditorMode;
    Ref<Shader> mObjectIdShader;

    EditorInterfaceHotReloader mInterfaceHotReloader;
    ResourceHotReloader mResourceHotReloader;
};

}
