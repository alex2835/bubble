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

constexpr u32 cDerived = FieldInfo::ReadOnly | FieldInfo::Transient;

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
        .Field<&LightComponent::mDistance>( "distance", { .mMin = 0.1f, .mMax = 3250.0f,
                                                          .mFlags = FieldInfo::Slider | FieldInfo::Logarithmic,
                                                          .mVisible = NotDirectional } )
        .Field<&LightComponent::mCutOff>( "cut_off", { .mMin = 0.0f, .mMax = 90.0f, .mFlags = FieldInfo::Slider,
                                                       .mVisible = IsSpot } )
        .Field<&LightComponent::mOuterCutOff>( "outer_cut_off", { .mMin = 0.0f, .mMax = 90.0f, .mFlags = FieldInfo::Slider,
                                                                  .mVisible = IsSpot } )
        // Made from the distance by OnChanged: shown, never set or saved.
        .Field<&LightComponent::mConstant>( "constant", { .mFlags = cDerived, .mVisible = NotDirectional } )
        .Field<&LightComponent::mLinear>( "linear", { .mFlags = cDerived, .mVisible = NotDirectional } )
        .Field<&LightComponent::mQuadratic>( "quadratic", { .mFlags = cDerived, .mVisible = NotDirectional } )
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
