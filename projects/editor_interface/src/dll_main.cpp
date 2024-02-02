
#include "GL/glew.h"
#include "imgui.h"
#include "hot_reloader_export.hpp"
#include "engine/utils/ieditor_interface.hpp"
#include "engine/utils/types.hpp"

#include "interface/entities_interface.hpp"


namespace bubble
{

void ImGuiContextInit( ImGuiContext* context )
{
    glewInit();
    ImGui::SetCurrentContext( context );
}
HR_REGISTER_FUNC( void, ImGuiContextInit, ImGuiContext* );


void LoadEditorInterface( std::vector<Ref<IEditorInterface>>& out )
{
    auto entitiesInterface = Ref<IEditorInterface>(
        ( IEditorInterface* ) new EntitiesInterface );
    out.push_back( entitiesInterface );


}
HR_REGISTER_FUNC( void, LoadEditorInterface, std::vector<Ref<IEditorInterface>>& );

}