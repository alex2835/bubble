#include "engine/pch/pch.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include "engine/scripting/lua_value_property.hpp"

namespace bubble
{
void CameraComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, CameraComponent& cameraComponent )
{
    ImGui::TextColored( TEXT_COLOR, "CameraComponent" );

    CheckboxField<CameraComponent>( ctx, entity, "Use Transform Propagation", &CameraComponent::mUseTransformPropagation );

    // Clipping planes. Each is clamped by the other, so the bounds are read
    // fresh rather than baked into the step.
    DragFloatField<CameraComponent>( ctx, entity, "Near", &CameraComponent::mNear, 0.01f, 0.01f, cameraComponent.mFar );
    DragFloatField<CameraComponent>( ctx, entity, "Far", &CameraComponent::mFar, 1.0f, cameraComponent.mNear, 10000.0f );

    SliderFloatField<CameraComponent>( ctx, entity, "FOV", &CameraComponent::mFov, 0.1f, 3.14f );

    DragFloatField<CameraComponent>( ctx, entity, "Max Speed", &CameraComponent::mMaxSpeed, 0.1f, 0.0f, 100.0f );
    DragFloatField<CameraComponent>( ctx, entity, "Mouse Sensitivity", &CameraComponent::mMouseSensitivity, 0.1f, 0.1f, 10.0f );
    DragFloatField<CameraComponent>( ctx, entity, "Radius", &CameraComponent::mRadius, 0.1f, 0.1f, 100.0f );

    // Update camera vectors when angles change
    cameraComponent.EulerAnglesToVectors();
}

void CameraComponent::ToJson( json& json, const Project& project, const CameraComponent& cameraComponent )
{
    json["Position"] = cameraComponent.mPosition;
    json["Forward"] = cameraComponent.mForward;
    json["Up"] = cameraComponent.mUp;
    json["Right"] = cameraComponent.mRight;
    json["WorldUp"] = cameraComponent.mWorldUp;
    json["Near"] = cameraComponent.mNear;
    json["Far"] = cameraComponent.mFar;
    json["Fov"] = cameraComponent.mFov;
    json["Yaw"] = cameraComponent.mYaw;
    json["Pitch"] = cameraComponent.mPitch;
    json["MaxSpeed"] = cameraComponent.mMaxSpeed;
    json["MouseSensitivity"] = cameraComponent.mMouseSensitivity;
    json["Radius"] = cameraComponent.mRadius;
    json["UseTransformPropagation"] = cameraComponent.mUseTransformPropagation;
}

void CameraComponent::FromJson( const json& json, Project& project, CameraComponent& cameraComponent )
{
    if ( json.contains( "Position" ) )
        cameraComponent.mPosition = json["Position"];

    if ( json.contains( "Forward" ) )
        cameraComponent.mForward = json["Forward"];

    if ( json.contains( "Up" ) )
        cameraComponent.mUp = json["Up"];

    if ( json.contains( "Right" ) )
        cameraComponent.mRight = json["Right"];

    if ( json.contains( "WorldUp" ) )
        cameraComponent.mWorldUp = json["WorldUp"];

    if ( json.contains( "Near" ) )
        cameraComponent.mNear = json["Near"];

    if ( json.contains( "Far" ) )
        cameraComponent.mFar = json["Far"];

    if ( json.contains( "Fov" ) )
        cameraComponent.mFov = json["Fov"];

    if ( json.contains( "Yaw" ) )
        cameraComponent.mYaw = json["Yaw"];

    if ( json.contains( "Pitch" ) )
        cameraComponent.mPitch = json["Pitch"];

    if ( json.contains( "MaxSpeed" ) )
        cameraComponent.mMaxSpeed = json["MaxSpeed"];

    if ( json.contains( "MouseSensitivity" ) )
        cameraComponent.mMouseSensitivity = json["MouseSensitivity"];

    if ( json.contains( "Radius" ) )
        cameraComponent.mRadius = json["Radius"];

    if ( json.contains( "UseTransformPropagation" ) )
        cameraComponent.mUseTransformPropagation = json["UseTransformPropagation"];

    cameraComponent.EulerAnglesToVectors();
}

void CameraComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<CameraComponent>(
        "Camera",
        sol::call_constructor,
        sol::constructors<CameraComponent(), CameraComponent( vec3, f32, f32, f32, vec3 )>(),

        // vec3 fields by value - see ValueProperty.
        "position",              ValueProperty( &CameraComponent::mPosition ),
        "forward",               ValueProperty( &CameraComponent::mForward ),
        "up",                    ValueProperty( &CameraComponent::mUp ),
        "right",                 ValueProperty( &CameraComponent::mRight ),
        "world_up",              ValueProperty( &CameraComponent::mWorldUp ),
        "near",                  &CameraComponent::mNear,
        "far",                   &CameraComponent::mFar,
        "fov",                   &CameraComponent::mFov,
        "yaw",                   &CameraComponent::mYaw,
        "pitch",                 &CameraComponent::mPitch,
        "max_speed",              &CameraComponent::mMaxSpeed,
        "mouse_sensitivity",      &CameraComponent::mMouseSensitivity,
        "center",                ValueProperty( &CameraComponent::mCenter ),
        "radius",                &CameraComponent::mRadius,
        "use_transform_propagation", &CameraComponent::mUseTransformPropagation,

        "get_lookat_mat",          &CameraComponent::GetLookatMat,
        "get_projection_mat",      &CameraComponent::GetProjectionMat,
        "euler_angles_to_vectors",  &CameraComponent::EulerAnglesToVectors,
        "update_orbit",           &CameraComponent::UpdateOrbit
    );
}

}
