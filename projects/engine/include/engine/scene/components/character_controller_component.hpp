#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/physics/character_controller.hpp"

namespace bubble
{
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
    explicit CharacterControllerComponent( CharacterController controller );
    ~CharacterControllerComponent();

    CharacterController mController;
};

}
