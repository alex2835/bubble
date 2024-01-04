#pragma once
#include "interface/ieditor_interface.hpp"
#include "app/scene_viewport.hpp"

namespace bubble
{
class EditorViewportInterface : IEditorInterface
{
public:
    EditorViewportInterface() = default;
    ~EditorViewportInterface() override = default;

    std::string Name() override
    {
        return "Viewport";
    }

    void OnInit() override
    {
    }


    void OnUpdate() override
    {
    }

    void OnDraw() override
    {
        ImGui::Begin( "name" );
        ImGui::Button( "test button", ImVec2{ 100, 200 } );
        ImGui::End();
    }

private:
    SceneViewport mViewport;
};

}
