#pragma once
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"
#include "engine/utils/types.hpp"

// Basic components
namespace bubble
{
struct TagComponent : public string
{
	static string_view Name()
	{
		return "TagComponent"sv;
	}
	static void OnComponentDraw( const Loader& loader, TagComponent& component );
	static void ToJson( const Loader& loader, json& json, const TagComponent& component );
	static void FromJson( Loader& loader, const json& json, TagComponent& component );
};

struct TransformComponent
{
	static string_view Name()
	{
		return "TransformComponent"sv;
	}
	static void OnComponentDraw( const Loader& loader, TransformComponent& component );
	static void ToJson( const Loader& loader, json& json, const TransformComponent& component );
	static void FromJson( Loader& loader, const json& json, TransformComponent& component );

	mat4 Transform();
	vec3 mPosition = vec3( 0 );
	vec3 mRotation = vec3( 0 );
	vec3 mScale = vec3( 1 );

};

struct LightComponent : public Light
{
	static string_view Name()
	{
		return "LightComponent"sv;
	}
	static void OnComponentDraw( const Loader& loader, LightComponent& component );
	static void ToJson( const Loader& loader, json& json, const LightComponent& component );
	static void FromJson( Loader& loader, const json& json, LightComponent& component );
};

struct ModelComponent : public Ref<Model>
{
	using Ref<Model>::operator=;

	static string_view Name()
	{
		return "ModelComponent"sv;
	}
	static void OnComponentDraw( const Loader& loader, ModelComponent& component );
	static void ToJson( const Loader& loader, json& json, const ModelComponent& component );
	static void FromJson( Loader& loader, const json& json, ModelComponent& component );
};

struct ShaderComponent : public Ref<Shader>
{
	using Ref<Shader>::operator=;

	static string_view Name()
	{
		return "ShaderComponent"sv;
	}
	static void OnComponentDraw( const Loader& loader, ShaderComponent& component );
	static void ToJson( const Loader& loader, json& json, const ShaderComponent& component );
	static void FromJson( Loader& loader, const json& json, ShaderComponent& component );
};

}