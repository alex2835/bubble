
#include <GL/glew.h>
#include <hot_reloader_export.hpp>
#include "engine/engine.hpp"
#include "editor_application.hpp"
#include "interface/entities_interface.hpp"
#include "interface/scnene_viewport_interface.hpp"
#include "interface/project_interface.hpp"
#include "interface/menubar.hpp"

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
    auto menubar = Ref<IEditorInterface>( ( IEditorInterface* ) new Menubar( editorState, engine ) );
    interfaces.push_back( menubar );

    auto entitiesInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new EntitiesInterface( editorState, engine ) );
    interfaces.push_back( entitiesInterface );

    auto viewportInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new SceneViewportInterface( editorState, engine ) );
    interfaces.push_back( viewportInterface );

    auto projectInterface = Ref<IEditorInterface>( ( IEditorInterface* ) new ProjectInterface( editorState, engine ) );
    interfaces.push_back( projectInterface );
}
HR_REGISTER_FUNC( void, LoadEditorInterface, 
                  EditorState&, Engine&, std::vector<Ref<IEditorInterface>>& );

}