#include "engine/pch/pch.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
void LightComponent::OnComponentDraw( const Project& project, const Entity& entity, LightComponent& lightComponent )
{
    ImGui::TextColored( TEXT_COLOR, "LightComponent" );

    // Light Type Selection
    const char* lightTypes[] = { "Directional", "Point", "Spot" };
    int currentType = static_cast<int>( lightComponent.mType );
    if ( ImGui::Combo( "Type", &currentType, lightTypes, IM_ARRAYSIZE( lightTypes ) ) )
    {
        lightComponent.mType = static_cast<LightType>( currentType );
    }

    // Color
    ImGui::ColorEdit3( "Color", &lightComponent.mColor.x );

    // Brightness
    ImGui::DragFloat( "Brightness", &lightComponent.mBrightness, 0.01f, 0.0f, 10.0f );

    // Type-specific properties
    if ( lightComponent.mType == LightType::Directional )
    {
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );
    }
    else if ( lightComponent.mType == LightType::Point )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        ImGui::SliderFloat( "Distance", &lightComponent.mDistance, 0.1f, 3250.0f, "%.2f", ImGuiSliderFlags_Logarithmic );

        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
    else if ( lightComponent.mType == LightType::Spot )
    {
        //ImGui::DragFloat3( "Position", &lightComponent.mPosition.x, 0.1f );
        //if ( ImGui::DragFloat3( "Direction", &lightComponent.mDirection.x, 0.01f ) )
        //    lightComponent.mDirection = normalize( lightComponent.mDirection );

        ImGui::SliderFloat( "Distance", &lightComponent.mDistance, 0.1f, 3250.0f, "%.2f", ImGuiSliderFlags_Logarithmic );

        ImGui::SliderFloat( "Cut Off", &lightComponent.mCutOff, 0.0f, 90.0f );
        ImGui::SliderFloat( "Outer Cut Off", &lightComponent.mOuterCutOff, 0.0f, 90.0f );

        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
}

void LightComponent::ToJson( json& json, const Project& project, const LightComponent& light )
{
    json["Type"] = static_cast<int>( light.mType );
    json["Color"] = { light.mColor.x, light.mColor.y, light.mColor.z };
    json["Brightness"] = light.mBrightness;
    json["Position"] = { light.mPosition.x, light.mPosition.y, light.mPosition.z };
    json["Direction"] = { light.mDirection.x, light.mDirection.y, light.mDirection.z };
    json["Distance"] = light.mDistance;
    json["CutOff"] = light.mCutOff;
    json["OuterCutOff"] = light.mOuterCutOff;
    json["Constant"] = light.mConstant;
    json["Linear"] = light.mLinear;
    json["Quadratic"] = light.mQuadratic;
}

void LightComponent::FromJson( const json& json, Project& project, LightComponent& lightComponent )
{
    if ( json.contains( "Type" ) )
        lightComponent.mType = static_cast<LightType>( json["Type"].get<int>() );

    if ( json.contains( "Color" ) )
    {
        auto color = json["Color"];
        lightComponent.mColor = vec3( color[0], color[1], color[2] );
    }

    if ( json.contains( "Brightness" ) )
        lightComponent.mBrightness = json["Brightness"];

    if ( json.contains( "Position" ) )
    {
        auto pos = json["Position"];
        lightComponent.mPosition = vec3( pos[0], pos[1], pos[2] );
    }

    if ( json.contains( "Direction" ) )
    {
        auto dir = json["Direction"];
        lightComponent.mDirection = vec3( dir[0], dir[1], dir[2] );
    }

    if ( json.contains( "Distance" ) )
        lightComponent.mDistance = json["Distance"];

    if ( json.contains( "CutOff" ) )
        lightComponent.mCutOff = json["CutOff"];

    if ( json.contains( "OuterCutOff" ) )
        lightComponent.mOuterCutOff = json["OuterCutOff"];

    if ( json.contains( "Constant" ) )
        lightComponent.mConstant = json["Constant"];

    if ( json.contains( "Linear" ) )
        lightComponent.mLinear = json["Linear"];

    if ( json.contains( "Quadratic" ) )
        lightComponent.mQuadratic = json["Quadratic"];
}

void LightComponent::CreateLuaBinding( sol::state& lua )
{
    constexpr string_view lightTypes = R"(
        LightType = 
        {
            directional = 0,
            point = 1,
            spot = 2
        }
    )";
    lua.safe_script( lightTypes );

    lua.new_usertype<LightComponent>(
        "Light",
        sol::call_constructor,
        sol::constructors<LightComponent()>(),

        "type",        &LightComponent::mType,
        "color",       &LightComponent::mColor,
        "brightness",  &LightComponent::mBrightness,
        "position",    &LightComponent::mPosition,
        "direction",   &LightComponent::mDirection,
        "distance",    &LightComponent::mDistance,
        "cut_off",      &LightComponent::mCutOff,
        "outer_cut_off", &LightComponent::mOuterCutOff,

        "create_dir_light",   &LightComponent::CreateDirLight,
        "create_point_light", &LightComponent::CreatePointLight,
        "create_spot_light",  &LightComponent::CreateSpotLight
    );
}

}
