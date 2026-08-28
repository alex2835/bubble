#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/renderer/camera.hpp"

namespace bubble
{
struct CameraComponent : public Camera
{
    using Camera::Camera;           // inherit Camera(vec3, f32, f32, f32, vec3) etc.
    CameraComponent() = default;
    explicit CameraComponent( const Camera& c ) : Camera( c ) {}

    static int ID() { return static_cast<int>( ComponentID::Camera ); }
	static string_view Name() { return "Camera"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, CameraComponent& component );
	static void ToJson( json& json, const Project& project, const CameraComponent& component );
	static void FromJson( const json& json, Project& project, CameraComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};

}
