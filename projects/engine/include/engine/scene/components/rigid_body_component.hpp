#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/physics/physics_engine.hpp"

namespace bubble
{
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

}
