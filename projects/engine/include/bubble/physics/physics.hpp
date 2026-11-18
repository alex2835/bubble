#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
class AssetRegistry;

// Colliders: the entity's shape for physics, about its origin, scaled by
// its world transform. With no rigid_body the entity stands still - the
// level's floors and walls -; with one it moves. One collider an entity:
// more is an error naming them, and the first in this order counts.
struct BoxCollider
{
    vec3 mSize{ 1.0f };
};

// A sphere: one radius, so the largest of the scale's axes counts.
struct SphereCollider
{
    f32 mRadius = 0.5f;
};

// Upright along Y, centred on the origin: `mHeight` from end to end, the
// rounded ends included. One shorter than twice its radius is a sphere.
struct CapsuleCollider
{
    f32 mRadius = 0.5f;
    f32 mHeight = 2.0f;
};

// A model's triangles, its finest level of detail: what a level's ground
// is. Never moves - Jolt simulates no mesh -, and waits for its model.
struct MeshCollider
{
    AssetId mModel;
};

// Makes the entity's collider move: simulated by gravity and what it
// meets, or, kinematic, moved by its transform - a platform that carries
// what stands on it and pushes what is in its way.
struct RigidBody
{
    f32 mMass = 1;
    f32 mFriction = 0.5f;
    f32 mRestitution = 0;
    f32 mLinearDamping = 0.05f;
    f32 mAngularDamping = 0.05f;
    f32 mGravityScale = 1;
    bool mKinematic = false;
};

// A character that walks: an upright capsule, its origin at its feet,
// moved by the velocity its script sets each step - gravity and jumps
// included, the controller adds none. It keeps to the ground, climbs steps
// up to `mStepHeight` and slopes up to `mMaxSlope`; riding a platform is
// adding the ground's velocity to its own. Its weight - `mass` kilograms
// under the world's gravity - presses on a body it stands on; 0 presses on
// nothing. Jolt holds about ten to one between masses touching, and a body
// far lighter shakes under the character until it seems to stand on nothing
// a step in four: hence 10, not a person's 70, beside bodies' 1.
struct CharacterController
{
    f32 mRadius = 0.4f;
    f32 mHeight = 1.8f;
    f32 mMaxSlope = 0.7853982f;
    f32 mStepHeight = 0.35f;
    f32 mMass = 10.0f;
};

// What a ray met first.
struct RayHit
{
    Entity mEntity;
    vec3 mPoint{ 0.0f };
    // The surface's, pointing back along the ray.
    vec3 mNormal{ 0.0f };
    f32 mDistance = 0;
};

// What a character controller found in its last step.
struct CharacterState
{
    vec3 mVelocity{ 0.0f };
    // Standing on something not too steep.
    bool mOnGround = false;
    vec3 mGroundNormal{ 0.0f };
    // How what it stands on moves: what riding a platform adds.
    vec3 mGroundVelocity{ 0.0f };
};

// A world's physics as the rest of the engine asks it - a service. What
// an entity has not - a body, a character - is nothing, or false.
class PhysicsWorld
{
public:
    virtual ~PhysicsWorld() = default;

    // Bodies only: a character is not in the way of a ray.
    virtual opt<RayHit> TryRaycast( vec3 from, vec3 to ) = 0;
    // The bodies touching a sphere, each once.
    virtual vector<Entity> OverlapSphere( vec3 centre, f32 radius ) = 0;

    virtual opt<vec3> TryVelocity( Entity entity ) = 0;
    virtual bool TrySetVelocity( Entity entity, vec3 velocity ) = 0;
    virtual opt<vec3> TryAngularVelocity( Entity entity ) = 0;
    virtual bool TrySetAngularVelocity( Entity entity, vec3 velocity ) = 0;
    // At the centre of mass; a force lasts the next step.
    virtual bool TryAddImpulse( Entity entity, vec3 impulse ) = 0;
    virtual bool TryAddForce( Entity entity, vec3 force ) = 0;

    virtual opt<CharacterState> TryCharacter( Entity entity ) = 0;
    // What it moves by in the steps to come, until set again.
    virtual bool TrySetCharacterVelocity( Entity entity, vec3 velocity ) = 0;

    virtual vec3 Gravity() const = 0;
};

// The colliders, rigid_body and character_controller on Jolt, stepped in
// Phase::Physics: what moved written to the entities' transforms, contacts
// to their scripts' on_collision; physics.raycast and physics.overlap_sphere
// and the bodies' and characters' velocities for scripts. A world's
// snapshot keeps Jolt's state, the bodies made again under their own ids.
ModuleType PhysicsModule( AssetRegistry& assets );
}
