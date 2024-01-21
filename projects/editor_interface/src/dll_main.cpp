
#include "GL/glew.h"
#include "imgui.h"
#include "hot_reloader_export.hpp"
#include "engine/utils/ieditor_interface.hpp"
#include "engine/utils/types.hpp"

#include "interface/editor_window_test.hpp"

using namespace bubble;

void ImGuiContextInit( ImGuiContext* context )
{
    glewInit();
    ImGui::SetCurrentContext( context );
}
HR_REGISTER_FUNC( void, ImGuiContextInit, ImGuiContext* );


void LoadEditorInterface( std::vector<Ref<IEditorInterface>>& out )
{
    auto testInterface = Ref<IEditorInterface>(
            ( IEditorInterface* ) new TestInterface() );

    out.push_back( testInterface );
}
HR_REGISTER_FUNC( void, LoadEditorInterface, std::vector<Ref<IEditorInterface>>& );
