#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/renderer/transform.hpp"

namespace bubble
{
struct TransformComponent : public Transform
{
    using Transform::Transform;  // aggregate-init ctors (C++20 parenthesized aggregate init)
    TransformComponent() = default;
    explicit TransformComponent( const Transform& t ) : Transform( t ) {}

    static int ID() { return static_cast<int>( ComponentID::Transform ); }
	static string_view Name() { return "Transform"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, TransformComponent& component );
    static void ToJson( json& json, const Project& project, const TransformComponent& component );
    static void FromJson( const json& json, Project& project, TransformComponent& component );
    static void CreateLuaBinding( sol::state& lua );
};

}
