#pragma once
#include "engine/loader/loader.hpp"
#include "engine/renderer/light.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "components_manager.hpp"


// Basic components
namespace bubble
{
struct TagComponent : public string
{
    static string_view Name()
    {
        return "TagComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, void* raw );
};

struct TransformComponent
{
    static string_view Name()
    {
        return "TransformComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, void* raw );

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
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, void* raw );
};

struct ModelComponent : public Ref<Model>
{
    using Ref<Model>::operator=;

    static string_view Name()
    {
        return "ModelComponent"sv;
    }
    BUBBLE_ENGINE_EXPORT static void OnComponentDraw( const Loader& loader, void* raw );
    BUBBLE_ENGINE_EXPORT static void ToJson( const Loader& loader, json& json, const void* raw );
    BUBBLE_ENGINE_EXPORT static void FromJson( Loader& loader, const json& json, void* raw );
};

}