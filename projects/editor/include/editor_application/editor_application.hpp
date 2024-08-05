#pragma once
#include "engine/engine.hpp"
#include "editor_state.hpp"
#include "editor_interfaces/editor_interface_hot_reloader.hpp"
#include "resources_hot_reloader/resources_hot_reloader.hpp"
#include "utils/scene_camera.hpp"
#undef APIENTRY

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
    void OpenProject( const path& projectPath );
    void Run();
    void DrawProjectScene();

private:
    EditorMode mEditorMode;
    // Shader to handle object picking in viewport
    Ref<Shader> mObjectIdShader;
    // Editor UI interfaces and resource hot reloader
    EditorInterfaceHotReloader mInterfaceHotReloader;
    ResourcesHotReloader mResourcesHotReloader;
};

}
