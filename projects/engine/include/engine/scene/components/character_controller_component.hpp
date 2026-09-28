#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/physics/character_controller.hpp"

namespace bubble
{
struct CharacterControllerComponent
{
    static int ID() { return static_cast<int>( ComponentID::CharacterController ); }
    static string_view Name() { return "character_controller"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, CharacterControllerComponent& component );
    static void ToJson( json& json, const Project& project, const CharacterControllerComponent& component );
    static void FromJson( const json& json, Project& project, CharacterControllerComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    CharacterControllerComponent();
    CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight = 0.35f );
    explicit CharacterControllerComponent( CharacterController controller );
    ~CharacterControllerComponent();
    // Copying makes a new Bullet controller; moving - which is also how the pool
    // relocates the component - keeps the one the physics world holds.
    CharacterControllerComponent( const CharacterControllerComponent& ) = default;
    CharacterControllerComponent& operator=( const CharacterControllerComponent& ) = default;
    CharacterControllerComponent( CharacterControllerComponent&& ) noexcept = default;
    CharacterControllerComponent& operator=( CharacterControllerComponent&& ) noexcept = default;

    CharacterController mController;
};

}
