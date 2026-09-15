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
void CameraComponent::UpdateOrbit( TransformComponent& transform )
{
    Camera::UpdateOrbit();

    // The look angles come from mForward, which UpdateOrbit pointed at
    // mCenter - not from mYaw/mPitch, which are the place on the sphere and
    // face the other way. This is the inverse of VectorsFromEuler, so the
    // propagation reproduces mForward exactly.
    transform.mPosition   = mPosition;
    transform.mRotation.x = std::asin( glm::clamp( mForward.y, -1.0f, 1.0f ) );
    transform.mRotation.y = std::atan2( mForward.z, mForward.x );
}

void CameraComponent::OrbitFromTransform( const TransformComponent& transform )
{
    // Inverse of Camera::UpdateOrbit:
    //   x = cx + r cos(p) sin(y),  y = cy - r sin(p),  z = cz + r cos(p) cos(y)
    const vec3 offset = transform.mPosition - mCenter;
    const f32 radius = length( offset );
    // On top of the center there is no direction to take; keep the angles
    // and just record the radius.
    if ( radius < 1e-4f )
    {
        mRadius = radius;
        return;
    }
    mRadius = radius;
    mYaw    = std::atan2( offset.x, offset.z );
    mPitch  = -std::asin( glm::clamp( offset.y / radius, -1.0f, 1.0f ) );
}

void CameraComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, CameraComponent& cameraComponent )
{
    ImGui::TextColored( TEXT_COLOR, "CameraComponent" );
    ImGui::TextWrapped( "Position and orientation come from this entity's TransformComponent." );

    // Clipping planes. Each is clamped by the other, so the bounds are read
    // fresh rather than baked into the step.
    DragFloatField<CameraComponent>( ctx, entity, "Near", &CameraComponent::mNear, 0.01f, 0.01f, cameraComponent.mFar );
    DragFloatField<CameraComponent>( ctx, entity, "Far", &CameraComponent::mFar, 1.0f, cameraComponent.mNear, 10000.0f );

    SliderFloatField<CameraComponent>( ctx, entity, "FOV", &CameraComponent::mFov, 0.1f, 3.14f );

    DragFloatField<CameraComponent>( ctx, entity, "Max Speed", &CameraComponent::mMaxSpeed, 0.1f, 0.0f, 100.0f );
    DragFloatField<CameraComponent>( ctx, entity, "Mouse Sensitivity", &CameraComponent::mMouseSensitivity, 0.1f, 0.1f, 10.0f );
    DragFloatField<CameraComponent>( ctx, entity, "Radius", &CameraComponent::mRadius, 0.1f, 0.1f, 100.0f );
}

// Position, forward, up and right are not written: they are the cache the
// transform fills, and the transform is serialized on its own.
void CameraComponent::ToJson( json& json, const Project& project, const CameraComponent& cameraComponent )
{
    json["WorldUp"] = cameraComponent.mWorldUp;
    json["Near"] = cameraComponent.mNear;
    json["Far"] = cameraComponent.mFar;
    json["Fov"] = cameraComponent.mFov;
    json["Yaw"] = cameraComponent.mYaw;
    json["Pitch"] = cameraComponent.mPitch;
    json["MaxSpeed"] = cameraComponent.mMaxSpeed;
    json["MouseSensitivity"] = cameraComponent.mMouseSensitivity;
    json["Radius"] = cameraComponent.mRadius;
}

void CameraComponent::FromJson( const json& json, Project& project, CameraComponent& cameraComponent )
{
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
}

void CameraComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<CameraComponent>(
        "Camera",
        sol::call_constructor,
        // Default constructed only. The Camera( position, yaw, pitch, fov, up )
        // constructor is not exposed: a component's position is its entity's
        // transform, and a position passed here would be overwritten on the
        // first frame. Set fov, near, far, radius and the rest as fields:
        //     local c = Camera(); c.fov = 1.2
        sol::constructors<CameraComponent()>(),

        // Read only: these are the cache filled from the entity's transform.
        // A script moves a camera by moving its entity, or with update_orbit.
        "position",              sol::readonly( &CameraComponent::mPosition ),
        "forward",               sol::readonly( &CameraComponent::mForward ),
        "up",                    sol::readonly( &CameraComponent::mUp ),
        "right",                 sol::readonly( &CameraComponent::mRight ),
        // vec3 fields by value - see ValueProperty.
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

        "get_lookat_mat",          &CameraComponent::GetLookatMat,
        "get_projection_mat",      &CameraComponent::GetProjectionMat,
        // camera:update_orbit( entity:get_transform() )
        "update_orbit",           &CameraComponent::UpdateOrbit,
        // camera:orbit_from_transform( entity:get_transform() ), in on_start
        "orbit_from_transform",   &CameraComponent::OrbitFromTransform
    );
}

}
