#pragma once
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"
#include "engine/utils/imexp.hpp"
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
	BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, TagComponent& component );
	BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const TagComponent& component );
	BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, TagComponent& component );
};

struct TransformComponent
{
	static string_view Name()
	{
		return "TransformComponent"sv;
	}
	BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, TransformComponent& component );
	BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const TransformComponent& component );
	BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, TransformComponent& component );

	BUBBLE_ENGINE_EXPORT mat4 Transform();
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
	BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, LightComponent& component );
	BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const LightComponent& component );
	BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, LightComponent& component );
};

struct ModelComponent : public Ref<Model>
{
	using Ref<Model>::operator=;

	static string_view Name()
	{
		return "ModelComponent"sv;
	}
	BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, ModelComponent& component );
	BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const ModelComponent& component );
	BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, ModelComponent& component );
};

struct ShaderComponent : public Ref<Shader>
{
	using Ref<Shader>::operator=;

	static string_view Name()
	{
		return "ShaderComponent"sv;
	}
	BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, ShaderComponent& component );
	BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const ShaderComponent& component );
	BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, ShaderComponent& component );
};

}