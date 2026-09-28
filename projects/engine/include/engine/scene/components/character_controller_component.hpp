#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/physics/character_controller.hpp"
#include <sol/sol.hpp>

namespace bubble
{
// A walking capsule, as data: its size and how it jumps, falls and climbs.
// The Bullet controller, mController, is made from these fields; the
// description leaves it out, so it is not saved or shown, and a copy makes
// its own.
//
// Speeds, slope and gravity reach the controller at once. The capsule is
// baked into the Bullet objects, so a new size makes the controller again:
// at once while it is in no physics world, else by
// Engine::RebuildChangedBodies before the next physics step - standing where
// the old one stood either way.
struct CharacterControllerComponent
{
    static int ID() { return static_cast<int>( ComponentID::CharacterController ); }
    static string_view Name() { return "character_controller"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    static void Reflect();
    // Lua: the fields come from Reflect(); these are what is added to them.
    static void BindLuaMethods( sol::state& lua, sol::usertype<CharacterControllerComponent>& type );

public:
    CharacterControllerComponent();
    CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight = 0.35f );
    ~CharacterControllerComponent();
    // Copying makes a new Bullet controller; moving - which is also how the
    // storage relocates the component - keeps the one the physics world holds.
    CharacterControllerComponent( const CharacterControllerComponent& ) = default;
    CharacterControllerComponent& operator=( const CharacterControllerComponent& ) = default;
    CharacterControllerComponent( CharacterControllerComponent&& ) noexcept = default;
    CharacterControllerComponent& operator=( CharacterControllerComponent&& ) noexcept = default;

    // After a change to the fields: brings the controller up to date, or
    // marks it for Engine::RebuildChangedBodies.
    void Apply();
    // The controller made again from the fields, where the old one stood. Not
    // while it is in a world.
    void Rebuild();

    f32 mRadius = 0.5f;
    f32 mHeight = 1.0f;       // between the two caps
    f32 mStepHeight = 0.35f;
    f32 mJumpSpeed = 10.0f;
    f32 mFallSpeed = 55.0f;
    f32 mMaxSlope = glm::radians( 45.0f );
    vec3 mGravity = vec3( 0, -30, 0 );

    CharacterController mController;
    // A size change made while in a world, waiting for the engine.
    bool mRebuild = false;
};

}
