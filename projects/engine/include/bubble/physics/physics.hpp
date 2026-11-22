#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/name.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/scene/entity_path.hpp"
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
//
// A body of several shapes is a rigid_body over entities with colliders:
// each collider under it - down to the next rigid_body - is part of its
// body where its transform puts it, the rigid_body's entity's own collider
// too if it has one. A part has no body of its own: its contacts are the
// body's, and its rays hit the body's entity.
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
// is. Never moves - Jolt simulates no mesh -, and waits for its model;
// under a rigid_body it is left out of the body.
struct MeshCollider
{
    AssetId mModel;
};

// A model's points wrapped tight, as a band would: a shape that moves, as
// a mesh_collider cannot - a barrel, a rock, a car's body.
struct ConvexCollider
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

// Holds the entity's body to another's, or to the world, so they move only
// as its kind lets them:
//   fixed        not at all: welded;
//   hinge        turning about `axis` through `anchor`: a door, a wheel;
//   slider       along `axis`: a drawer, a lift;
//   distance     keeping `anchor` from the other end - `connected_anchor` -
//                between `min` and `max`, or as far as it is when both are
//                0: a rope, a rod, with `spring` an elastic;
//   swing_twist  swinging off `axis` up to `swing` and twisting about it
//                between `min` and `max`: a shoulder, a hip.
// `anchor` and `axis` are in the entity's own space, which need not be the
// body's: a joint on a part of a body holds the body. How the two stand
// when the joint is made is its rest: a hinge's 0, a slider's 0. Limits
// equal are none; a hinge's and a twist's are radians from -pi to pi, the
// lower at most 0 and the upper at least 0, as a slider's metres are.
struct Joint
{
    Name mKind = "hinge"_n;
    // What it holds the entity to, from the entity: "../frame",
    // "/level/hook". None: the world, where the joint is.
    EntityPath mConnected{};
    vec3 mAnchor{ 0.0f };
    vec3 mAxis{ 1.0f, 0.0f, 0.0f };
    // distance: the other end, in the connected entity's space - in the
    // world's when it holds to the world.
    vec3 mConnectedAnchor{ 0.0f };
    f32 mMin = 0;
    f32 mMax = 0;
    // swing_twist: the most it swings off its axis, radians.
    f32 mSwing = 0.5f;
    // A limit that gives: how many times a second it springs back, and how
    // soon it settles - 0 rigid.
    f32 mSpring = 0;
    f32 mDamping = 0.5f;
    // hinge, slider, swing_twist: off, velocity - `target` radians or
    // metres a second -, or position - `target` radians or metres from the
    // rest -; with at most `max_force` newtons, or newton-metres turning.
    Name mMotor = "off"_n;
    f32 mTarget = 0;
    f32 mMaxForce = 1000;
    // What the joint holds before it breaks, newtons: 0 never. A joint
    // broken is gone until it is made again, and its scripts hear
    // on_joint_break.
    f32 mBreakForce = 0;
    // Whether the two bodies it holds collide with each other.
    bool mCollide = false;
};

// A body on wheels, driven: its engine and gearbox, and the driver's hands
// and feet, which a script sets each frame. Its wheels are the entities
// under it with a vehicle_wheel. Forward is its -Z, up its +Y.
struct Vehicle
{
    // Newton-metres at the engine, up to `max_rpm`; the gears are chosen by
    // themselves.
    f32 mEngineTorque = 500;
    f32 mMaxRpm = 6000;
    // Where its weight is, from its origin: lower, it rolls over less.
    vec3 mMassOffset{ 0.0f };
    // The most it pitches or rolls, radians; pi lets it overturn.
    f32 mMaxTilt = 3.14159265f;
    // The driver: forward and back from -1 to 1, the wheel from -1 left to
    // 1 right, the brake and the hand brake from 0 to 1.
    f32 mThrottle = 0;
    f32 mSteer = 0;
    f32 mBrake = 0;
    f32 mHandbrake = 0;
};

// A vehicle's wheel, where its entity is: its middle with the suspension
// let all the way down. The entity's children turn, steer and spring with
// it - a model of a wheel, its axle along X. The vehicle drives the wheels
// with `drive` in pairs, left and right, an axle at a time.
struct VehicleWheel
{
    f32 mRadius = 0.35f;
    f32 mWidth = 0.25f;
    // How far it rises from where it is placed, metres.
    f32 mSuspension = 0.3f;
    // How many times a second the suspension springs back, and how soon it
    // settles.
    f32 mStiffness = 1.5f;
    f32 mDamping = 0.5f;
    // The most it turns, radians; 0 does not steer.
    f32 mMaxSteer = 0;
    bool mDrive = true;
    bool mHandbrake = false;
    f32 mBrakeTorque = 1500;
    // The tyre's friction, as many times as a road car's.
    f32 mGrip = 1;
};

// A skinned model's bones as bodies. Off, its animator poses it; on, they
// fall and tumble from the pose they had - a fall, a death -, and the model
// is drawn as they lie, the entity itself staying where it is. A bone is a
// capsule from its joint to the child joint that goes on its way; a bone
// shorter than `min_bone` - a finger, a toe - rides on the one above. A bone
// named as Mixamo names them - spine, neck, head, shoulder, arm, forearm,
// hand, up leg, leg, foot - keeps a human's limits: the spine stiff, the head
// turning only so far, a knee bending back and an elbow forward from
// straight, however bent they were; one of another name swings up to `swing`
// and twists up to `twist` either way, from how it stood when it came on.
struct Ragdoll
{
    bool mActive = false;
    // Kilograms, shared among the bones by their size.
    f32 mMass = 70;
    // A capsule's radius, as a share of its bone's length.
    f32 mThickness = 0.25f;
    // Metres.
    f32 mMinBone = 0.06f;
    f32 mSwing = 0.7f;
    f32 mTwist = 0.4f;
};

// What a vehicle's gauges show.
struct VehicleGauges
{
    // Metres a second, forward; back is less than 0.
    f32 mSpeed = 0;
    f32 mRpm = 0;
    // 0 neutral, below 0 reverse.
    i32 mGear = 0;
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

    // Whether the entity's joint gave way; none while it is not made.
    virtual opt<bool> TryJointBroken( Entity entity ) = 0;
    // The entity's vehicle's gauges; none while it is not made.
    virtual opt<VehicleGauges> TryVehicle( Entity entity ) = 0;
    // An impulse shared by an active ragdoll's bones as they weigh: false
    // when it is not on.
    virtual bool TryPushRagdoll( Entity entity, vec3 impulse ) = 0;
};

// The colliders, rigid_body, character_controller and joint on Jolt, stepped in
// Phase::Physics: what moved written to the entities' transforms, contacts
// to their scripts' on_collision; physics.raycast and physics.overlap_sphere
// and the bodies' and characters' velocities for scripts. A world's
// snapshot keeps Jolt's state, the bodies made again under their own ids.
ModuleType PhysicsModule( AssetRegistry& assets );
}
