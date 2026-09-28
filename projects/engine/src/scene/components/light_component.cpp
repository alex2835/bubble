#include "engine/pch/pch.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include "engine/reflection/reflection.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scripting/lua_value_property.hpp"

namespace bubble
{
void LightComponent::SyncToTransform( const TransformComponent& transform )
{
    mPosition = transform.World().mPosition;
    // Forward is down in local space.
    mDirection = transform.World().RotationMat() * vec4( 0, -1, 0, 0 );
    Update();
}

void LightComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, LightComponent& lightComponent )
{
    ImGui::TextColored( TEXT_COLOR, "LightComponent" );

    // Light Type Selection
    static const char* lightTypes[] = { "Directional", "Point", "Spot" };
    EditField<LightComponent>( ctx, entity, "Type", &LightComponent::mType, []( LightType& type )
    {
        int current = static_cast<int>( type );
        if ( not ImGui::Combo( "Type", &current, lightTypes, IM_ARRAYSIZE( lightTypes ) ) )
            return false;
        type = static_cast<LightType>( current );
        return true;
    } );

    ColorEdit3Field<LightComponent>( ctx, entity, "Color", &LightComponent::mColor );
    DragFloatField<LightComponent>( ctx, entity, "Brightness", &LightComponent::mBrightness, 0.01f, 0.0f, 10.0f );

    // Type-specific properties
    if ( lightComponent.mType == LightType::Point or lightComponent.mType == LightType::Spot )
    {
        SliderFloatField<LightComponent>( ctx, entity, "Distance", &LightComponent::mDistance, 0.1f, 3250.0f, "%.2f", ImGuiSliderFlags_Logarithmic );
    }
    if ( lightComponent.mType == LightType::Spot )
    {
        SliderFloatField<LightComponent>( ctx, entity, "Cut Off", &LightComponent::mCutOff, 0.0f, 90.0f );
        SliderFloatField<LightComponent>( ctx, entity, "Outer Cut Off", &LightComponent::mOuterCutOff, 0.0f, 90.0f );
    }
    if ( lightComponent.mType != LightType::Directional )
    {
        // Show calculated attenuation values (read-only)
        ImGui::Text( "Attenuation:" );
        ImGui::Indent();
        ImGui::Text( "Constant: %.3f", lightComponent.mConstant );
        ImGui::Text( "Linear: %.4f", lightComponent.mLinear );
        ImGui::Text( "Quadratic: %.6f", lightComponent.mQuadratic );
        ImGui::Unindent();
    }
}

namespace
{
bool NotDirectional( const entt::meta_any& light )
{
    return light.cast<const LightComponent&>().mType != LightType::Directional;
}

bool IsSpot( const entt::meta_any& light )
{
    return light.cast<const LightComponent&>().mType == LightType::Spot;
}

void LightChanged( LightComponent& light )
{
    light.Update();
}
}

// What the light is. Where it is and where it points come from the entity's
// transform (SyncToTransform), and the attenuation from the distance - none
// of it is saved.
void LightComponent::Reflect()
{
    ReflectEnum<LightType>();
    TypeBuilder<LightComponent>( "Light" )
        .Field<&LightComponent::mType>( "type" )
        .Field<&LightComponent::mColor>( "color", { .mFlags = FieldInfo::Color } )
        .Field<&LightComponent::mBrightness>( "brightness", { .mMin = 0.0f, .mMax = 10.0f, .mSpeed = 0.01f } )
        .Field<&LightComponent::mDistance>( "distance", { .mMin = 0.1f, .mMax = 3250.0f, .mVisible = NotDirectional } )
        .Field<&LightComponent::mCutOff>( "cut_off", { .mMin = 0.0f, .mMax = 90.0f, .mVisible = IsSpot } )
        .Field<&LightComponent::mOuterCutOff>( "outer_cut_off", { .mMin = 0.0f, .mMax = 90.0f, .mVisible = IsSpot } )
        .OnChanged<&LightChanged>();
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
        // vec3 fields by value - see ValueProperty.
        "color",       ValueProperty( &LightComponent::mColor ),
        "brightness",  &LightComponent::mBrightness,
        "position",    ValueProperty( &LightComponent::mPosition ),
        "direction",   ValueProperty( &LightComponent::mDirection ),
        "distance",    &LightComponent::mDistance,
        "cut_off",      &LightComponent::mCutOff,
        "outer_cut_off", &LightComponent::mOuterCutOff,

        "create_dir_light",   &LightComponent::CreateDirLight,
        "create_point_light", &LightComponent::CreatePointLight,
        "create_spot_light",  &LightComponent::CreateSpotLight
    );
}

}
