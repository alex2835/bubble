
#include <GL/glew.h>
#include <hot_reloader_export.hpp>
#include "engine/engine.hpp"
#include "editor_application.hpp"
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
    auto menuBar = Ref<IEditorInterface>( ( IEditorInterface* ) new Menubar( editorState ) );
    interfaces.push_back( menuBar );

    auto entitiesInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new EntitiesInterface( editorState ) );
    interfaces.push_back( entitiesInterface );

    auto viewportInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new SceneViewportInterface( editorState ) );
    interfaces.push_back( viewportInterface );

    auto projectInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new ProjectInterface( editorState ) );
    interfaces.push_back( projectInterface );
}
HR_REGISTER_FUNC( void, LoadEditorInterface, EditorState&, vector<Ref<IEditorInterface>>& );

}