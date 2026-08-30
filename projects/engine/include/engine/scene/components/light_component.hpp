#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
struct LightComponent : public Light
{
    using Light::Light;             // inherit any Light constructors
    LightComponent() = default;
    explicit LightComponent( const Light& l ) : Light( l ) {}

    // Light's factories return Light by value. Inheriting them unchanged would
    // hand Lua a bubble::Light, which has no usertype registered, so every field
    // access on the result fails. These shadow them with the bound type.
    static LightComponent CreateDirLight()   { return LightComponent( Light::CreateDirLight() ); }
    static LightComponent CreatePointLight() { return LightComponent( Light::CreatePointLight() ); }
    static LightComponent CreateSpotLight()  { return LightComponent( Light::CreateSpotLight() ); }

    static int ID() { return static_cast<int>( ComponentID::Light ); }
	static string_view Name() { return "Light"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, LightComponent& component );
	static void ToJson( json& json, const Project& project, const LightComponent& component );
	static void FromJson( const json& json, Project& project, LightComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};

}
