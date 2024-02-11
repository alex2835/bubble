
#include "GL/glew.h"
#include "imgui.h"
#include "hot_reloader_export.hpp"
#include "engine/engine.hpp"
#include "editor_app.hpp"
#include "interface/entities_interface.hpp"
#include "interface/scnene_viewport_interface.hpp"


namespace bubble
{

void ImGuiContextInit( ImGuiContext* context )
{
    glewInit();
    ImGui::SetCurrentContext( context );
}
HR_REGISTER_FUNC( void, ImGuiContextInit, ImGuiContext* );


void LoadEditorInterface( EditorState& editorState,
                          Engine& engine,
                          std::vector<Ref<IEditorInterface>>& interfaces )
{
    auto entitiesInterface = Ref<IEditorInterface>(
        ( IEditorInterface* ) new EntitiesInterface( editorState, engine ) );
    interfaces.push_back( entitiesInterface );

    auto viewportInterface = Ref<IEditorInterface>(
        ( IEditorInterface* ) new SceneViewportInterface( editorState, engine ) );
    interfaces.push_back( viewportInterface );

}
HR_REGISTER_FUNC( void, LoadEditorInterface, 
                  EditorState&, Engine&, std::vector<Ref<IEditorInterface>>& );

}