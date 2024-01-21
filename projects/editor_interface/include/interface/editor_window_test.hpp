#pragma once
#include "imgui.h"
#include "engine/utils/ieditor_interface.hpp"

namespace bubble
{
class TestInterface : public IEditorInterface
{
public:
    TestInterface()
    {

    }

    ~TestInterface() override
    {

    }

    string_view Name() override
    {
        return "Test window";
    }

    void OnInit() override
    {

    }

    void OnUpdate( DeltaTime ) override
    {
    }

    void OnDraw() override
    {
        ImGui::Begin( Name().data(), &mOpen );
        {
            ImGui::Button( "Button", { 200, 100 } );
        }
        ImGui::End();
    }

private:
};

}
