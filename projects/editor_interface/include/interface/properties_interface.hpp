#pragma once
#include "imgui.h"
#include "engine/utils/ieditor_interface.hpp"

namespace bubble
{
class PropertiesInterface : public IEditorInterface
{
public:
    PropertiesInterface()
    {

    }

    ~PropertiesInterface() override
    {

    }

    string_view Name() override
    {
        return "Properties";
    }

    void OnInit() override
    {

    }

    void OnUpdate( DeltaTime ) override
    {
    }

    void OnDraw( Engine& ) override
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
