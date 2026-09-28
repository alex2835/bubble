#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/physics/physics_engine.hpp"
#include <sol/sol.hpp>

namespace bubble
{
enum class BodyShape
{
    Sphere,
    Box,
    Capsule
};

// A body the physics world simulates, as data: its shape and how it moves.
// The Bullet body, mRigidBody, is made from these fields; the description
// leaves it out, so it is not saved or shown, and a copy makes its own.
//
// A change to a field reaches the Bullet body at once while the body is in no
// physics world - in the editor, or before it is added. A body in a world is
// taken out, made again and put back by Engine::RebuildChangedBodies before
// the next physics step, which is where mass and shape changes wait.
// Friction and the kinematic flag apply at once either way.
struct RigidBodyComponent
{
    static int ID() { return static_cast<int>( ComponentID::RigidBody ); }
    static string_view Name() { return "rigid_body"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    static void Reflect();
    // Lua: the fields come from Reflect(); these are what is added to them.
    static void BindLuaMethods( sol::state& lua, sol::usertype<RigidBodyComponent>& type );

public:
    RigidBodyComponent();
    ~RigidBodyComponent();
    // Copying makes a new Bullet body; moving - which is also how the storage
    // relocates the component - keeps the one the physics world holds.
    RigidBodyComponent( const RigidBodyComponent& ) = default;
    RigidBodyComponent& operator=( const RigidBodyComponent& ) = default;
    RigidBodyComponent( RigidBodyComponent&& ) noexcept = default;
    RigidBodyComponent& operator=( RigidBodyComponent&& ) noexcept = default;

    // After a change to the fields: brings the Bullet body up to date, or
    // marks it for Engine::RebuildChangedBodies. A kinematic body's mass
    // is made 0 here - Bullet only moves a massless body kinematically.
    void Apply();
    // The Bullet body made again from the fields. Not while it is in a world.
    void Rebuild();

    BodyShape mShape = BodyShape::Sphere;
    f32 mRadius = 1.0f;             // sphere, capsule
    f32 mHeight = 1.0f;             // capsule: between the two caps
    vec3 mHalfExtents = vec3( 1 );  // box
    f32 mMass = 0.0f;               // 0: static
    f32 mFriction = 0.5f;
    // Moved by set_transform from a script instead of by forces, and pushes
    // the dynamic bodies it meets.
    bool mKinematic = false;

    RigidBody mRigidBody;
    // A change made while the body was in a world, waiting for the engine.
    bool mRebuild = false;
};

}
