#pragma once
#include "engine/engine.hpp"
#include "editor_application/editor_state.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "editor_resources_hot_reloader/resources_hot_reloader.hpp"

namespace bubble
{
class BubbleEditor : public EditorState
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
    // Editor user interface and resource hot reloader
    EditorUserInterface mEditorUserInterface;
    ResourcesHotReloader mResourcesHotReloader;
};

}
