#pragma once
#include "engine/utils/types.hpp"
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"
#include "component_manager.hpp"
#include "engine/utils/imexp.hpp"


// Basic components
namespace bubble
{
struct TagComponent : public string
{
    static string_view Name()
    {
        return "TagComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( json& j, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( const json& j, void* raw );
};

struct TransformComponent
{
    static string_view Name()
    {
        return "TransformComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( json& j, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( const json& j, void* raw );

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
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( json& j, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( const json& j, void* raw );
};

struct ModelComponent : public Ref<Model>
{
    static string_view Name()
    {
        return "ModelComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( json& j, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( const json& j, void* raw );
};

}