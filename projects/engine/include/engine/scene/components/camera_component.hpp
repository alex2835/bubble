#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
// A camera on an entity.
//
// The entity's TransformComponent is where the camera is and where it looks:
// mPosition and mRotation (pitch, yaw) there are the truth, authored by the
// gizmo, by a script moving the entity, or by UpdateOrbit below. The Camera
// fields this inherits - mPosition, mForward, mRight, mUp - are a cache that
// Engine::PropagateCameraTransforms fills from the transform every frame, for
// the renderer and for scripts that want the basis vectors. Writing them
// directly changes nothing that lasts.
//
// mYaw/mPitch on a component are the orbit's place on the sphere around
// mCenter, driven by the script, and are not the look direction.
struct CameraComponent : public Camera
{
    using Camera::Camera;           // inherit Camera(vec3, f32, f32, f32, vec3) etc.
    CameraComponent() = default;
    explicit CameraComponent( const Camera& c ) : Camera( c ) {}

    // The orbit: mYaw/mPitch/mRadius around mCenter, written into the
    // transform as position and look angles. Hides Camera::UpdateOrbit(),
    // which only fills the cache - a component has no position of its own
    // to update.
    void UpdateOrbit( TransformComponent& transform );

    static int ID() { return static_cast<int>( ComponentID::Camera ); }
	static string_view Name() { return "Camera"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, CameraComponent& component );
	static void ToJson( json& json, const Project& project, const CameraComponent& component );
	static void FromJson( const json& json, Project& project, CameraComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};

}
