
#include <GL/glew.h>
#include <hot_reloader_export.hpp>
#include "engine/engine.hpp"
#include "editor_application/editor_application.hpp"
#include "interfaces/entities_interface.hpp"
#include "interfaces/scnene_viewport_interface.hpp"
#include "interfaces/project_interface.hpp"
#include "interfaces/menubar.hpp"

namespace bubble
{
void ImGuiContextInit( ImGuiContext* context )
{
    glewInit();
    ImGui::SetCurrentContext( context );
}
HR_REGISTER_FUNC( void, ImGuiContextInit, ImGuiContext* );


void LoadEditorInterface( EditorState& editorState,
                          vector<Ref<IEditorInterface>>& interfaces )
{
    interfaces.push_back( Ref<IEditorInterface>( new Menubar( editorState ) ) );
    interfaces.push_back( Ref<IEditorInterface>( new EntitiesInterface( editorState ) ) );
    interfaces.push_back( Ref<IEditorInterface>( new SceneViewportInterface( editorState ) ) );
    interfaces.push_back( Ref<IEditorInterface>( new ProjectInterface( editorState ) ) );
}
HR_REGISTER_FUNC( void, LoadEditorInterface, EditorState&, vector<Ref<IEditorInterface>>& );

}