#pragma once
#include <sol/forward.hpp>
#include <sol/function.hpp>
#include "engine/types/string.hpp"
#include "engine/types/number.hpp"
#include "engine/types/json.hpp"
#include "engine/types/glm.hpp"
#include "engine/renderer/light.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/physics/physics_engine.hpp"

// Basic components
namespace bubble
{
class Project;
struct Model;
struct Script;

enum class ComponentID
{
	Tag,
	Transform,
	Camera,
	Model,
	Light,
	Shader,
	Script,
	Physics
};


struct TagComponent
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Tag ); }
	static string_view Name() { return "Tag"sv; }

	static void OnComponentDraw( const Project& project, TagComponent& component );
	static void ToJson( json& json, const Project& project, const TagComponent& component );
	static void FromJson( const json& json, Project& project, TagComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	TagComponent() = default;
	TagComponent( string name );
	string mName;
};


struct TransformComponent
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Transform ); }
	static string_view Name() { return "Transform"sv; }

    static void OnComponentDraw( const Project& project, TransformComponent& component );
    static void ToJson( json& json, const Project& project, const TransformComponent& component );
    static void FromJson( const json& json, Project& project, TransformComponent& component );
    static void CreateLuaBinding( sol::state& lua );
	
public:
	TransformComponent() = default;
    TransformComponent( vec3 pos, vec3 rot, vec3 scale );
	mat4 TransformMat();
	vec3 mPosition = vec3( 0 );
	vec3 mRotation = vec3( 0 );
	vec3 mScale = vec3( 1 );
};


struct CameraComponent : Camera
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Camera ); }
	static string_view Name() { return "Camera"sv; }

    static void OnComponentDraw( const Project& project, CameraComponent& component );
	static void ToJson( json& json, const Project& project, const CameraComponent& component );
	static void FromJson( const json& json, Project& project, CameraComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};


struct LightComponent : public Light
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Light ); }
	static string_view Name() { return "Light"sv; }

    static void OnComponentDraw( const Project& project, LightComponent& component );
	static void ToJson( json& json, const Project& project, const LightComponent& component );
	static void FromJson( const json& json, Project& project, LightComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};


struct ModelComponent : public Ref<Model>
{
	using Ref<Model>::operator=;
    static size_t ID() { return static_cast<size_t>( ComponentID::Model ); }
	static string_view Name() { return "Model"sv; }

	static void OnComponentDraw( const Project& project, ModelComponent& component );
	static void ToJson( json& json, const Project& project, const ModelComponent& component );
	static void FromJson( const json& json, Project& project, ModelComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};


struct ShaderComponent : public Ref<Shader>
{
	using Ref<Shader>::operator=;

    static size_t ID() { return static_cast<size_t>( ComponentID::Shader ); }
	static string_view Name() { return "Shader"sv; }

    static void OnComponentDraw( const Project& project, ShaderComponent& component );
	static void ToJson( json& json, const Project& project, const ShaderComponent& component );
	static void FromJson( const json& json, Project& project, ShaderComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};


struct ScriptComponent
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Script ); }
	static string_view Name() { return "Script"sv; }

    static void OnComponentDraw( const Project& project, ScriptComponent& component );
	static void ToJson( json& json, const Project& project, const ScriptComponent& component );
	static void FromJson( const json& json, Project& project, ScriptComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
	ScriptComponent() = default;
	ScriptComponent( const Ref<Script>& scirpt );
	~ScriptComponent();
	Ref<Script> mScript;
	sol::function mOnUpdate;
	//Ref<sol::lua_value> mState;
};


struct PhysicsComponent
{
    static size_t ID() { return static_cast<size_t>( ComponentID::Physics ); }
    static string_view Name() { return "Physics"sv; }

    static void OnComponentDraw( const Project& project, PhysicsComponent& component );
	static void ToJson( json& json, const Project& project, const PhysicsComponent& component );
	static void FromJson( const json& json, Project& project, PhysicsComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    PhysicsComponent() = default;
    PhysicsComponent( const Ref<PhysicsObject>& physicsObject );
    ~PhysicsComponent();

	Ref<PhysicsObject> mPhysicsObject;
};

}