#pragma once
#include <sol/forward.hpp>
#include <sol/function.hpp>
#include "engine/types/string.hpp"
#include "engine/types/number.hpp"
#include "engine/types/json.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/dynamic.hpp"
#include "engine/renderer/light.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/transform.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/physics/character_controller.hpp"

namespace recs
{
class Entity;
}

// Basic components
namespace bubble
{
using namespace recs;
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
	RigidBody,
	CharacterController,
	State
};


struct TagComponent
{
    static int ID() { return static_cast<int>( ComponentID::Tag ); }
	static string_view Name() { return "Tag"sv; }

	static void OnComponentDraw( const Project& project, const Entity& entity, TagComponent& component );
	static void ToJson( json& json, const Project& project, const TagComponent& component );
	static void FromJson( const json& json, Project& project, TagComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	TagComponent() = default;
	TagComponent( string name, string cls = "Object"s );
	string mName;
	string mClass;
};



struct TransformComponent : public Transform
{
    static int ID() { return static_cast<int>( ComponentID::Transform ); }
	static string_view Name() { return "Transform"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, TransformComponent& component );
    static void ToJson( json& json, const Project& project, const TransformComponent& component );
    static void FromJson( const json& json, Project& project, TransformComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};



struct CameraComponent : public Camera
{
    static int ID() { return static_cast<int>( ComponentID::Camera ); }
	static string_view Name() { return "Camera"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, CameraComponent& component );
	static void ToJson( json& json, const Project& project, const CameraComponent& component );
	static void FromJson( const json& json, Project& project, CameraComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};



struct LightComponent : public Light
{
    static int ID() { return static_cast<int>( ComponentID::Light ); }
	static string_view Name() { return "Light"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, LightComponent& component );
	static void ToJson( json& json, const Project& project, const LightComponent& component );
	static void FromJson( const json& json, Project& project, LightComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};



struct ModelComponent
{
    static int ID() { return static_cast<int>( ComponentID::Model ); }
	static string_view Name() { return "Model"sv; }

	static void OnComponentDraw( const Project& project, const Entity& entity, ModelComponent& component );
	static void ToJson( json& json, const Project& project, const ModelComponent& component );
	static void FromJson( const json& json, Project& project, ModelComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	ModelComponent() = default;
	ModelComponent( const Ref<Model>& model );
    ~ModelComponent();
	Ref<Model> mModel;
};



struct ShaderComponent
{
    static int ID() { return static_cast<int>( ComponentID::Shader ); }
	static string_view Name() { return "Shader"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, ShaderComponent& component );
	static void ToJson( json& json, const Project& project, const ShaderComponent& component );
	static void FromJson( const json& json, Project& project, ShaderComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	ShaderComponent() = default;
	ShaderComponent( const Ref<Shader>& shader );
    ~ShaderComponent();
    Ref<Shader> mShader;
};



struct ScriptComponent
{
    static int ID() { return static_cast<int>( ComponentID::Script ); }
	static string_view Name() { return "Script"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, ScriptComponent& component );
	static void ToJson( json& json, const Project& project, const ScriptComponent& component );
	static void FromJson( const json& json, Project& project, ScriptComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    ScriptComponent() = default;
    ScriptComponent( const Ref<Script>& scirpt );
    ~ScriptComponent();
    Ref<Script> mScript;
    sol::function mOnUpdate;
};



struct RigidBodyComponent
{
    static int ID() { return static_cast<int>( ComponentID::RigidBody ); }
    static string_view Name() { return "RigidBody"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, RigidBodyComponent& component );
    static void ToJson( json& json, const Project& project, const RigidBodyComponent& component );
    static void FromJson( const json& json, Project& project, RigidBodyComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    RigidBodyComponent();
    RigidBodyComponent( RigidBody rigidBody );
    ~RigidBodyComponent();

    RigidBody mRigidBody;
};



struct CharacterControllerComponent
{
    static int ID() { return static_cast<int>( ComponentID::CharacterController ); }
    static string_view Name() { return "CharacterController"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, CharacterControllerComponent& component );
    static void ToJson( json& json, const Project& project, const CharacterControllerComponent& component );
    static void FromJson( const json& json, Project& project, CharacterControllerComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    CharacterControllerComponent();
    CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight = 0.35f );
    ~CharacterControllerComponent();

    CharacterController mController;
};



struct StateComponent
{
    static int ID() { return static_cast<int>( ComponentID::State ); }
    static string_view Name() { return "State"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, StateComponent& component );
    static void ToJson( json& json, const Project& project, const StateComponent& component );
    static void FromJson( const json& json, Project& project, StateComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    StateComponent();
	StateComponent( Any any );
	StateComponent( const StateComponent& );
	StateComponent& operator= ( const StateComponent& );
	~StateComponent();
    Scope<Any> mState;
};


}