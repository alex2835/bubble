#pragma once
#include "ieditor_interface.hpp"
#include "editor_application.hpp"

namespace bubble
{
class ProjectInterface : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;
    ~ProjectInterface() override = default;

    string_view Name() override
    {
        return "Project"sv;
    }

    void OnInit() override
    {
    }

    void OnUpdate( DeltaTime ) override
    {
     
    }

    void OnDraw( DeltaTime ) override
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
        {

        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:

};

}
