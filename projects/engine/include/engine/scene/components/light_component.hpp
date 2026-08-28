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

    static int ID() { return static_cast<int>( ComponentID::Light ); }
	static string_view Name() { return "Light"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, LightComponent& component );
	static void ToJson( json& json, const Project& project, const LightComponent& component );
	static void FromJson( const json& json, Project& project, LightComponent& component );
	static void CreateLuaBinding( sol::state& lua );
};

}
