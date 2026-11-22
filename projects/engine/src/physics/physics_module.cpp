#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "jolt.hpp"
#include "physics_joints.hpp"
#include "physics_ragdoll.hpp"
#include "physics_vehicle.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/RegisterTypes.h>
#include <cstdarg>
#include <cstdio>

namespace bubble
{
namespace physics
{
Pose PoseOf( const mat4& matrix )
{
    Pose pose;
    pose.mPosition = vec3( matrix[3] );
    mat3 axes( matrix );
    for ( int axis = 0; axis < 3; ++axis )
    {
        pose.mScale[axis] = glm::length( axes[axis] );
        if ( pose.mScale[axis] > 0.0f )
            axes[axis] /= pose.mScale[axis];
    }
    pose.mRotation = glm::normalize( glm::quat_cast( axes ) );
    return pose;
}
}

namespace
{
using namespace physics;

// ---- Jolt, once a process ---------------------------------------------------

void JoltTrace( const char* format, ... )
{
    char text[512];
    va_list args;
    va_start( args, format );
    std::vsnprintf( text, sizeof( text ), format, args );
    va_end( args );
    LogInfo( "Jolt: {}", text );
}

#ifdef JPH_ENABLE_ASSERTS
bool JoltAssertFailed( const char* expression, const char* message, const char* file, JPH::uint line )
{
    LogError( "Jolt: {} {} ({}:{})", expression, message ? message : "", file, line );
    // Logged and gone on from: the test or the game says what came of it.
    return false;
}
#endif

// Jolt's factory and type registry are its own globals, made once and kept
// for the process.
void StartJolt()
{
    static const bool cStarted = [] {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = &JoltTrace;
        JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = &JoltAssertFailed; )
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        return true;
    }();
    (void)cStarted;
}

// A member that starts Jolt before the members that need it are made.
struct JoltStarted
{
    JoltStarted() { StartJolt(); }
};

// ---- layers: what meets what ------------------------------------------------

namespace Layers
{
// What stands still meets only what moves.
constexpr JPH::ObjectLayer cStatic = 0;
constexpr JPH::ObjectLayer cMoving = 1;
}

const JPH::BroadPhaseLayer cStaticBroad( 0 );
const JPH::BroadPhaseLayer cMovingBroad( 1 );

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
{
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer( JPH::ObjectLayer layer ) const override
    {
        return layer == Layers::cStatic ? cStaticBroad : cMovingBroad;
    }
#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
    const char* GetBroadPhaseLayerName( JPH::BroadPhaseLayer layer ) const override
    {
        return layer == cStaticBroad ? "static" : "moving";
    }
#endif
};

class LayerVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide( JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad ) const override
    {
        return layer != Layers::cStatic or broad == cMovingBroad;
    }
};

class LayerPairs final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide( JPH::ObjectLayer a, JPH::ObjectLayer b ) const override
    {
        return a != Layers::cStatic or b != Layers::cStatic;
    }
};

// Snapshots keep what moves: what stands still is made again as it was.
class MovingOnly final : public JPH::StateRecorderFilter
{
public:
    bool ShouldSaveBody( const JPH::Body& body ) const override { return not body.IsStatic(); }
};

// How large the entity is in the world: its scale and its ancestors', as
// they are written. A matrix's axes would give it with rounding - 0.9999999
// for a turned unit box - and a body made again would not be the body it
// was.
vec3 WorldScale( const Scene& scene, Entity entity )
{
    vec3 scale( 1.0f );
    for ( Entity at = entity; scene.Alive( at ); at = ParentOf( scene, at ) )
        if ( const auto transformMaybe = scene.TryGet<Transform>( at ) )
            scale *= transformMaybe->mScale;
    return scale;
}

// The parent's world matrix: what a body's world pose is local to.
mat4 ParentMatrix( const Scene& scene, Entity entity )
{
    const Entity parent = ParentOf( scene, entity );
    return parent == Entity::Null ? mat4( 1.0f ) : ComputeWorldMatrix( scene, parent );
}

bool HasCollider( const Scene& scene, Entity entity )
{
    return scene.Has<BoxCollider>( entity ) or scene.Has<SphereCollider>( entity ) or
           scene.Has<CapsuleCollider>( entity ) or scene.Has<ConvexCollider>( entity ) or
           scene.Has<MeshCollider>( entity );
}

// The entity whose body a collider belongs to: the nearest with a
// rigid_body, itself or up the tree; with none above, itself.
Entity BodyOwnerOf( const Scene& scene, Entity entity )
{
    for ( Entity at = entity; scene.Alive( at ); at = ParentOf( scene, at ) )
        if ( scene.Has<RigidBody>( at ) )
            return at;
    return entity;
}

// The colliders under `entity` that are parts of its body: down to the next
// rigid_body, whose own they are.
void CollectParts( const Scene& scene, Entity entity, vector<Entity>& parts )
{
    for ( const Entity child : ChildrenOf( scene, entity ) )
    {
        if ( scene.Has<RigidBody>( child ) )
            continue;
        if ( HasCollider( scene, child ) )
            parts.push_back( child );
        CollectParts( scene, child, parts );
    }
}

// Where `part` is in `owner`'s body, which is unscaled: the transforms on
// the way down from it, under the owner's scale. Worked out from the
// transforms as written and not from the owner's pose, so a body made again
// from the same data - a snapshot's - is made the same to the last bit.
mat4 PlacementIn( const Scene& scene, Entity owner, Entity part )
{
    mat4 placement( 1.0f );
    for ( Entity at = part; at != owner and scene.Alive( at ); at = ParentOf( scene, at ) )
        if ( const auto transformMaybe = scene.TryGet<Transform>( at ) )
            placement = LocalMatrix( *transformMaybe ) * placement;
    return glm::scale( mat4( 1.0f ), WorldScale( scene, owner ) ) * placement;
}

// True while it lives.
struct ScopedFlag
{
    explicit ScopedFlag( bool& flag ) : mFlag( flag ) { mFlag = true; }
    ~ScopedFlag() { mFlag = false; }
    ScopedFlag( const ScopedFlag& ) = delete;
    ScopedFlag& operator=( const ScopedFlag& ) = delete;

    bool& mFlag;
};

// A contact as Jolt reports it, gathered during the step for the scripts
// after it.
struct Contact
{
    JPH::uint64 mFirst = 0;
    JPH::uint64 mSecond = 0;
    vec3 mPoint{ 0.0f };
    // From the first towards the second.
    vec3 mNormal{ 0.0f };
};

class ContactLog final : public JPH::ContactListener
{
public:
    void OnContactAdded( const JPH::Body& first,
                         const JPH::Body& second,
                         const JPH::ContactManifold& manifold,
                         JPH::ContactSettings& ) override
    {
        if ( manifold.mRelativeContactPointsOn1.empty() )
            return;
        const lock_guard lock( mMutex );
        mAdded.push_back( Contact{ first.GetUserData(), second.GetUserData(),
                                   FromJolt( JPH::Vec3( manifold.GetWorldSpaceContactPointOn1( 0 ) ) ),
                                   FromJolt( manifold.mWorldSpaceNormal ) } );
    }

    vector<Contact> Take()
    {
        const lock_guard lock( mMutex );
        return std::exchange( mAdded, {} );
    }

private:
    mutex mMutex;
    vector<Contact> mAdded;
};

// The pairs of bodies a joint keeps from colliding. A body's group is its
// entity, its subgroup 0.
class JointedPairs final : public JPH::GroupFilter
{
public:
    bool CanCollide( const JPH::CollisionGroup& first, const JPH::CollisionGroup& second ) const override
    {
        const auto found = mApart.find( Key( first ) );
        return found == mApart.end() or ranges::find( found->second, Key( second ) ) == found->second.end();
    }

    // A body by its entity, and the bone of a ragdoll from 1.
    static u64 KeyOf( Entity entity, u32 bone = 0 ) { return ( UserDataOf( entity ) << 32 ) | bone; }

    void Keep( u64 first, u64 second, bool apart )
    {
        Set( first, second, apart );
        Set( second, first, apart );
    }

    void Clear() { mApart.clear(); }

private:
    static u64 Key( const JPH::CollisionGroup& group )
    {
        return ( static_cast<u64>( group.GetGroupID() ) << 32 ) | group.GetSubGroupID();
    }

    // Each pair once a joint: two joints between the same bodies count twice.
    void Set( u64 from, u64 key, bool apart )
    {
        vector<u64>& others = mApart[from];
        if ( apart )
            others.push_back( key );
        else if ( const auto found = ranges::find( others, key ); found != others.end() )
            others.erase( found );
    }

    hmap<u64, vector<u64>> mApart;
};

// What a constraint in Jolt is of: its entity, and which of the entity's -
// its joint, its vehicle. Jolt's user data on the constraint.
enum class Slot : u32
{
    Joint = 0,
    Vehicle = 1,
    // A ragdoll, and its bones' joints after: 2 + the bone.
    Ragdoll = 2,
};

u64 KeyOf( Entity entity, Slot slot )
{
    return UserDataOf( entity ) | ( static_cast<u64>( slot ) << 32 );
}

Slot SlotOf( u64 key )
{
    return static_cast<Slot>( key >> 32 );
}

// A constraint as a snapshot keeps it, in the order Jolt holds them - the
// order it restores them by -; a joint with its frame on the connected body
// as it was when the joint was made.
struct KeptJoint
{
    u64 mKey = 0;
    JointFrame mConnectedFrame;
};

// The bodies' state as a snapshot keeps it: Jolt's own bytes, which entity
// had which body, to make them again under their ids, and the joints in the
// order Jolt holds them.
class PhysicsSnapshot final : public ModuleSnapshot
{
public:
    PhysicsSnapshot( string state,
                     vector<pair<Entity, JPH::uint32>> bodies,
                     vector<KeptJoint> joints,
                     vector<pair<u64, JPH::uint32>> bones )
        : mState( std::move( state ) ),
          mBodies( std::move( bodies ) ),
          mJoints( std::move( joints ) ),
          mBones( std::move( bones ) )
    {
    }

    const string& State() const { return mState; }
    const vector<pair<Entity, JPH::uint32>>& Bodies() const { return mBodies; }
    const vector<KeptJoint>& Joints() const { return mJoints; }
    // A ragdoll's bones' bodies, by JointedPairs::KeyOf.
    const vector<pair<u64, JPH::uint32>>& Bones() const { return mBones; }

private:
    string mState;
    vector<pair<Entity, JPH::uint32>> mBodies;
    vector<KeptJoint> mJoints;
    vector<pair<u64, JPH::uint32>> mBones;
};

// An active ragdoll's bodies and joints, bone by bone; none while it is off
// or its model loads.
struct RagdollState
{
    AssetRef<ModelAsset> mModel;
    RagdollSkeleton mSkeleton;
    vector<JPH::BodyID> mBodies;
    vector<JPH::Ref<JPH::TwoBodyConstraint>> mJoints;
    // From the model's units to metres.
    f32 mScale = 1;
    // The joints' poses, worked in each frame.
    vector<mat4> mPoses;
    bool mSaid = false;
};

// A joint's constraint and what it was made between; none while it waits for
// a body.
struct JointState
{
    JPH::Ref<JPH::TwoBodyConstraint> mConstraint;
    Entity mFirst;
    // Entity::Null: the world.
    Entity mSecond;
    // The connected body's frame from the joint's first making: the rest it
    // keeps when a body is made again.
    opt<JointFrame> mConnectedFrame;
    // The two bodies kept from colliding while it holds.
    bool mApart = false;
    // What stops it is said once, until the joint changes.
    bool mSaid = false;
};

// A vehicle's constraint and its wheels, in its order; none while it waits
// for its body.
struct VehicleState
{
    JPH::Ref<JPH::VehicleConstraint> mConstraint;
    vector<pair<Entity, WheelPlace>> mWheels;
    bool mSaid = false;
};

// The wheels under a vehicle, down to the next rigid_body.
void CollectWheels( const Scene& scene, Entity entity, vector<Entity>& wheels )
{
    for ( const Entity child : ChildrenOf( scene, entity ) )
    {
        if ( scene.Has<RigidBody>( child ) )
            continue;
        if ( scene.Has<VehicleWheel>( child ) )
            wheels.push_back( child );
        CollectWheels( scene, child, wheels );
    }
}

// The vehicle a wheel is under.
Entity VehicleOf( const Scene& scene, Entity wheel )
{
    for ( Entity at = ParentOf( scene, wheel ); scene.Alive( at ); at = ParentOf( scene, at ) )
        if ( scene.Has<Vehicle>( at ) )
            return at;
    return Entity::Null;
}

// ---- the module ---------------------------------------------------------------

class Physics final : public Module, public PhysicsWorld
{
public:
    Physics( World& world, AssetRegistry& assets ) : mWorld( world ), mAssets( assets )
    {
        mSystem.Init( 65536, 0, 65536, 16384, mBroadPhaseLayers, mLayerVsBroadPhase, mLayerPairs );
        mSystem.SetContactListener( &mContacts );

        const auto rebuild = [this]( Entity entity ) { MarkDirty( entity ); };
        Watch<BoxCollider>( { "size"_n }, rebuild );
        Watch<SphereCollider>( { "radius"_n }, rebuild );
        Watch<CapsuleCollider>( { "radius"_n, "height"_n }, rebuild );
        Watch<ConvexCollider>( { "model"_n }, rebuild );
        Watch<MeshCollider>( { "model"_n }, rebuild );
        Watch<RigidBody>( { "mass"_n, "friction"_n, "restitution"_n, "linear_damping"_n, "angular_damping"_n,
                            "gravity_scale"_n, "kinematic"_n },
                          rebuild );
        // A rigid_body coming or going takes in, or lets go of, the
        // colliders under it.
        const auto regroup = [this]( Entity entity ) {
            const Scene& scene = mWorld.GetScene();
            if ( not scene.Alive( entity ) )
                return;
            vector<Entity> under;
            CollectParts( scene, entity, under );
            for ( const Entity part : under )
                MarkDirty( part );
        };
        world.OnAdded<RigidBody>( regroup );
        world.OnRemoved<RigidBody>( regroup );
        Watch<CharacterController>( { "radius"_n, "height"_n, "max_slope"_n, "step_height"_n, "mass"_n }, rebuild );
        world.OnAdded<Transform>( rebuild );
        world.OnRemoved<Transform>( rebuild );
        world.OnChanged<Transform>( "scale"_n, rebuild );
        // A joint's shape made again, its rest taken anew; its motor set as it
        // goes.
        const auto rejoin = [this]( Entity entity ) { MarkJoint( entity, true ); };
        Watch<Joint>( { "kind"_n, "connected"_n, "anchor"_n, "axis"_n, "connected_anchor"_n, "min"_n, "max"_n,
                        "swing"_n, "spring"_n, "damping"_n, "collide"_n },
                      rejoin );
        const auto drive = [this]( Entity entity ) { Drive( entity ); };
        world.OnChanged<Joint>( "motor"_n, drive );
        world.OnChanged<Joint>( "target"_n, drive );
        world.OnChanged<Joint>( "max_force"_n, drive );
        // A vehicle's weight moved makes its body again, and the vehicle
        // with it.
        const auto revehicle = [this]( Entity entity ) {
            MarkDirty( entity );
            MarkConstraint( KeyOf( entity, Slot::Vehicle ) );
        };
        Watch<Vehicle>( { "engine_torque"_n, "max_rpm"_n, "mass_offset"_n, "max_tilt"_n }, revehicle );
        const auto rewheel = [this]( Entity wheel ) {
            Entity vehicle = Entity::Null;
            if ( const auto found = mWheelOf.find( wheel ); found != mWheelOf.end() )
                vehicle = found->second;
            if ( mWorld.GetScene().Alive( wheel ) )
                if ( const Entity above = VehicleOf( mWorld.GetScene(), wheel ); above != Entity::Null )
                {
                    if ( vehicle != Entity::Null and vehicle != above )
                        MarkConstraint( KeyOf( vehicle, Slot::Vehicle ) );
                    vehicle = above;
                }
            if ( vehicle != Entity::Null )
                MarkConstraint( KeyOf( vehicle, Slot::Vehicle ) );
        };
        Watch<VehicleWheel>( { "radius"_n, "width"_n, "suspension"_n, "stiffness"_n, "damping"_n, "max_steer"_n,
                               "drive"_n, "handbrake"_n, "brake_torque"_n, "grip"_n },
                             rewheel );
        const auto reragdoll = [this]( Entity entity ) { MarkConstraint( KeyOf( entity, Slot::Ragdoll ) ); };
        Watch<Ragdoll>( { "active"_n, "mass"_n, "thickness"_n, "min_bone"_n, "swing"_n, "twist"_n }, reragdoll );
        world.AddStep( Phase::Transforms, [this]( f32 ) { PoseRagdolls(); } );
        const auto teleport = [this]( Entity entity ) { Teleport( entity ); };
        world.OnChanged<Transform>( "position"_n, teleport );
        world.OnChanged<Transform>( "rotation"_n, teleport );

        world.AddStep( Phase::Physics, [this]( f32 dt ) { Step( dt ); } );
        world.GetServices().Add<PhysicsWorld>( *this );
    }

    ~Physics() override
    {
        // Joints, vehicles, ragdolls and characters first: they hold bodies
        // and the system.
        RemoveVehicles();
        RemoveRagdolls();
        for ( auto& [entity, joint] : mJoints )
            if ( joint.mConstraint )
                mSystem.RemoveConstraint( joint.mConstraint );
        mJoints.clear();
        mCharacters.clear();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const auto& [entity, bodyId] : mBodies )
        {
            bodies.RemoveBody( bodyId );
            bodies.DestroyBody( bodyId );
        }
    }

    // ---- snapshots -------------------------------------------------------------

    Ref<const ModuleSnapshot> TrySnapshot() override
    {
        BuildDirty();
        JPH::StateRecorderImpl recorder;
        const MovingOnly filter;
        mSystem.SaveState( recorder, JPH::EStateRecorderState::All, &filter );
        for ( const Entity entity : mWorld.GetScene().EntitiesWith<CharacterController>() )
            if ( const auto found = mCharacters.find( entity ); found != mCharacters.end() )
                found->second->SaveState( recorder );
        vector<pair<Entity, JPH::uint32>> bodies;
        for ( const auto& [entity, bodyId] : mBodies )
            bodies.emplace_back( entity, bodyId.GetIndexAndSequenceNumber() );
        vector<KeptJoint> joints;
        for ( const JPH::Ref<JPH::Constraint>& constraint : mSystem.GetConstraints() )
        {
            const u64 key = constraint->GetUserData();
            KeptJoint kept{ .mKey = key, .mConnectedFrame = {} };
            if ( SlotOf( key ) == Slot::Joint )
                kept.mConnectedFrame = mJoints.at( EntityOf( key ) ).mConnectedFrame.value();
            joints.push_back( kept );
        }
        vector<pair<u64, JPH::uint32>> bones;
        for ( const auto& [entity, ragdoll] : mRagdolls )
            for ( u32 bone = 0; bone < ragdoll.mBodies.size(); ++bone )
                bones.emplace_back( JointedPairs::KeyOf( entity, bone + 1 ),
                                    ragdoll.mBodies[bone].GetIndexAndSequenceNumber() );
        return CreateRef<PhysicsSnapshot>( recorder.GetData(), std::move( bodies ), std::move( joints ),
                                           std::move( bones ) );
    }

    void Restoring( OptRef<const ModuleSnapshot> snapshotMaybe ) override
    {
        // Every body goes now, so none holds an id a kept one comes back
        // under; the scene's components coming back mark what to make.
        for ( auto& [entity, joint] : mJoints )
            if ( joint.mConstraint )
                mSystem.RemoveConstraint( joint.mConstraint );
        mJoints.clear();
        RemoveVehicles();
        RemoveRagdolls();
        mDirtyConstraints.clear();
        mDirtyConstraintSet.clear();
        mKeptOrder.clear();
        mKeptFrames.clear();
        mPairs->Clear();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const auto& [entity, bodyId] : mBodies )
        {
            bodies.RemoveBody( bodyId );
            bodies.DestroyBody( bodyId );
        }
        mBodies.clear();
        mPartOf.clear();
        mCharacters.clear();
        mWaiting.clear();
        mRestoringState.reset();
        mKeptIds.clear();
        if ( not snapshotMaybe )
            return;
        // Made by this module's type, in this world or another of the App.
        const auto& snapshot = static_cast<const PhysicsSnapshot&>( *snapshotMaybe );
        mRestoringState = snapshot.State();
        for ( const auto& [entity, bodyId] : snapshot.Bodies() )
            mKeptIds[entity] = JPH::BodyID( bodyId );
        for ( const auto& [key, bodyId] : snapshot.Bones() )
            mKeptBones[key] = JPH::BodyID( bodyId );
        for ( u32 order = 0; order < snapshot.Joints().size(); ++order )
        {
            const KeptJoint& kept = snapshot.Joints()[order];
            mKeptOrder[kept.mKey] = order;
            // A ragdoll is made whole: at the place of its first joint.
            if ( SlotOf( kept.mKey ) > Slot::Ragdoll )
                mKeptOrder.try_emplace( KeyOf( EntityOf( kept.mKey ), Slot::Ragdoll ), order );
            if ( SlotOf( kept.mKey ) == Slot::Joint )
                mKeptFrames[EntityOf( kept.mKey )] = kept.mConnectedFrame;
        }
    }

    void Restored() override
    {
        // Under their own ids first, so no new body takes one of theirs.
        BuildDirty();
        if ( mRestoringState )
        {
            JPH::StateRecorderImpl recorder;
            recorder.WriteBytes( mRestoringState->data(), mRestoringState->size() );
            recorder.Rewind();
            const MovingOnly filter;
            if ( not mSystem.RestoreState( recorder, &filter ) )
                LogError( "physics: the snapshot's bodies are not the world's: it goes on from the scene" );
            else
                for ( const Entity entity : mWorld.GetScene().EntitiesWith<CharacterController>() )
                    if ( const auto found = mCharacters.find( entity ); found != mCharacters.end() )
                        found->second->RestoreState( recorder );
        }
        mRestoringState.reset();
        mKeptIds.clear();
        mKeptOrder.clear();
        mKeptFrames.clear();
        mKeptBones.clear();
    }

    // ---- PhysicsWorld ------------------------------------------------------------

    opt<RayHit> TryRaycast( vec3 from, vec3 to ) override
    {
        BuildDirty();
        const JPH::RRayCast ray{ JPH::RVec3( ToJolt( from ) ), ToJolt( to - from ) };
        JPH::RayCastResult hit;
        if ( not mSystem.GetNarrowPhaseQuery().CastRay( ray, hit ) )
            return nullopt;
        const JPH::BodyLockRead lock( mSystem.GetBodyLockInterface(), hit.mBodyID );
        if ( not lock.Succeeded() )
            return nullopt;
        const JPH::Body& body = lock.GetBody();
        const JPH::RVec3 point = ray.GetPointOnRay( hit.mFraction );
        return RayHit{ .mEntity = EntityOf( body.GetUserData() ),
                       .mPoint = FromJolt( JPH::Vec3( point ) ),
                       .mNormal = FromJolt( body.GetWorldSpaceSurfaceNormal( hit.mSubShapeID2, point ) ),
                       .mDistance = hit.mFraction * glm::length( to - from ) };
    }

    vector<Entity> OverlapSphere( vec3 centre, f32 radius ) override
    {
        BuildDirty();
        JPH::SphereShape sphere( radius );
        sphere.SetEmbedded();
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        mSystem.GetNarrowPhaseQuery().CollideShape( &sphere, JPH::Vec3::sReplicate( 1.0f ),
                                                    JPH::RMat44::sTranslation( JPH::RVec3( ToJolt( centre ) ) ),
                                                    JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector );
        vector<Entity> touched;
        const JPH::BodyLockInterface& locks = mSystem.GetBodyLockInterface();
        for ( const JPH::CollideShapeResult& hit : collector.mHits )
        {
            const JPH::BodyLockRead lock( locks, hit.mBodyID2 );
            if ( not lock.Succeeded() )
                continue;
            const Entity entity = EntityOf( lock.GetBody().GetUserData() );
            if ( ranges::find( touched, entity ) == touched.end() )
                touched.push_back( entity );
        }
        return touched;
    }

    opt<vec3> TryVelocity( Entity entity ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( not bodyMaybe )
            return nullopt;
        return FromJolt( mSystem.GetBodyInterface().GetLinearVelocity( *bodyMaybe ) );
    }

    bool TrySetVelocity( Entity entity, vec3 velocity ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( bodyMaybe )
            mSystem.GetBodyInterface().SetLinearVelocity( *bodyMaybe, ToJolt( velocity ) );
        return bodyMaybe.has_value();
    }

    opt<vec3> TryAngularVelocity( Entity entity ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( not bodyMaybe )
            return nullopt;
        return FromJolt( mSystem.GetBodyInterface().GetAngularVelocity( *bodyMaybe ) );
    }

    bool TrySetAngularVelocity( Entity entity, vec3 velocity ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( bodyMaybe )
            mSystem.GetBodyInterface().SetAngularVelocity( *bodyMaybe, ToJolt( velocity ) );
        return bodyMaybe.has_value();
    }

    bool TryAddImpulse( Entity entity, vec3 impulse ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( bodyMaybe )
            mSystem.GetBodyInterface().AddImpulse( *bodyMaybe, ToJolt( impulse ) );
        return bodyMaybe.has_value();
    }

    bool TryAddForce( Entity entity, vec3 force ) override
    {
        const auto bodyMaybe = TryMovingBody( entity );
        if ( bodyMaybe )
            mSystem.GetBodyInterface().AddForce( *bodyMaybe, ToJolt( force ) );
        return bodyMaybe.has_value();
    }

    opt<CharacterState> TryCharacter( Entity entity ) override
    {
        BuildDirty();
        const auto found = mCharacters.find( entity );
        if ( found == mCharacters.end() )
            return nullopt;
        const JPH::CharacterVirtual& character = *found->second;
        return CharacterState{ .mVelocity = FromJolt( character.GetLinearVelocity() ),
                               .mOnGround = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround,
                               .mGroundNormal = FromJolt( character.GetGroundNormal() ),
                               .mGroundVelocity = FromJolt( character.GetGroundVelocity() ) };
    }

    bool TrySetCharacterVelocity( Entity entity, vec3 velocity ) override
    {
        BuildDirty();
        const auto found = mCharacters.find( entity );
        if ( found != mCharacters.end() )
            found->second->SetLinearVelocity( ToJolt( velocity ) );
        return found != mCharacters.end();
    }

    vec3 Gravity() const override { return FromJolt( mSystem.GetGravity() ); }

    opt<VehicleGauges> TryVehicle( Entity entity ) override
    {
        BuildDirty();
        const auto found = mVehicles.find( entity );
        const auto bodyFound = mBodies.find( entity );
        if ( found == mVehicles.end() or not found->second.mConstraint or bodyFound == mBodies.end() )
            return nullopt;
        const JPH::Body& body = *mSystem.GetBodyLockInterfaceNoLock().TryGetBody( bodyFound->second );
        return GaugesOf( *found->second.mConstraint, body );
    }

    bool TryPushRagdoll( Entity entity, vec3 impulse ) override
    {
        BuildDirty();
        const auto found = mRagdolls.find( entity );
        if ( found == mRagdolls.end() or found->second.mBodies.empty() )
            return false;
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        const RagdollState& ragdoll = found->second;
        for ( u32 bone = 0; bone < ragdoll.mBodies.size(); ++bone )
            bodies.AddImpulse( ragdoll.mBodies[bone], ToJolt( impulse * ragdoll.mSkeleton.mBones[bone].mShare ) );
        return true;
    }

    opt<bool> TryJointBroken( Entity entity ) override
    {
        BuildDirty();
        const auto found = mJoints.find( entity );
        if ( found == mJoints.end() or not found->second.mConstraint )
            return nullopt;
        return not found->second.mConstraint->GetEnabled();
    }

private:
    template <typename T>
    void Watch( initializer_list<Name> fields, const function<void( Entity )>& fn )
    {
        mWorld.OnAdded<T>( fn );
        mWorld.OnRemoved<T>( fn );
        for ( const Name field : fields )
            mWorld.OnChanged<T>( field, fn );
    }

    // In the order they came: bodies are made in an order a replay repeats.
    void MarkDirty( Entity entity )
    {
        if ( mDirtySet.insert( entity ).second )
            mDirty.push_back( entity );
        // A part gone is not found in the tree any more: its body is.
        if ( const auto found = mPartOf.find( entity ); found != mPartOf.end() and found->second != entity )
            MarkDirty( found->second );
    }

    void BuildDirty()
    {
        // A build asks after bodies itself - their velocities - and finds
        // them as they are.
        if ( mBuilding )
            return;
        const ScopedFlag building( mBuilding );
        if ( mDirty.empty() )
        {
            BuildConstraints();
            return;
        }
        // Those with ids kept by a snapshot first, in the order their ids
        // were given - the order they were made in -, and no new body may
        // take one of their ids.
        const auto keptEnd = stable_partition( mDirty.begin(), mDirty.end(),
                                               [this]( Entity entity ) { return mKeptIds.contains( entity ); } );
        sort( mDirty.begin(), keptEnd,
              [this]( Entity a, Entity b ) { return mKeptIds.at( a ).GetIndex() < mKeptIds.at( b ).GetIndex(); } );
        const vector<Entity> dirty = std::exchange( mDirty, {} );
        mDirtySet.clear();
        // A part of another's body is built as that body; what it had of
        // its own goes.
        const Scene& scene = mWorld.GetScene();
        vector<Entity> owners;
        hset<Entity> seen;
        for ( const Entity entity : dirty )
        {
            Entity owner = entity;
            if ( scene.Alive( entity ) and not scene.Has<CharacterController>( entity ) and
                 HasCollider( scene, entity ) )
                owner = BodyOwnerOf( scene, entity );
            if ( owner != entity )
            {
                RemoveBody( entity );
                mCharacters.erase( entity );
            }
            if ( seen.insert( owner ).second )
                owners.push_back( owner );
        }
        for ( const Entity owner : owners )
            Build( owner );
        // A body made may be what a joint waited for.
        for ( auto& [entity, joint] : mJoints )
            if ( not joint.mConstraint )
                MarkJoint( entity, false );
        for ( auto& [entity, vehicle] : mVehicles )
            if ( not vehicle.mConstraint )
                MarkConstraint( KeyOf( entity, Slot::Vehicle ) );
        for ( auto& [entity, ragdoll] : mRagdolls )
            if ( ragdoll.mBodies.empty() )
                MarkConstraint( KeyOf( entity, Slot::Ragdoll ) );
        BuildConstraints();
    }

    // The entity's body and character as its components say now: what was
    // there goes, and what there is to make is made.
    void Build( Entity entity )
    {
        const vec3 velocity = TryVelocity( entity ).value_or( vec3( 0.0f ) );
        const vec3 angular = TryAngularVelocity( entity ).value_or( vec3( 0.0f ) );
        RemoveBody( entity );
        mCharacters.erase( entity );
        // Held through the build: the registry keeps a model only while
        // someone does, and the shape is made from it below.
        AssetRef<ModelAsset> held;
        if ( const auto waiting = mWaiting.find( entity ); waiting != mWaiting.end() )
        {
            held = std::move( waiting->second );
            mWaiting.erase( waiting );
        }
        const Scene& scene = mWorld.GetScene();
        if ( not scene.Alive( entity ) )
            return;
        if ( const auto controllerMaybe = scene.TryGet<CharacterController>( entity ) )
            MakeCharacter( entity, *controllerMaybe );
        else if ( auto madeMaybe = TryMakeBody( entity ); not madeMaybe )
            LogError( "{}: {}", Describe( scene, entity ), madeMaybe.error() );
        else if ( *madeMaybe )
        {
            const JPH::BodyID bodyId = mBodies.at( entity );
            if ( mSystem.GetBodyInterface().GetMotionType( bodyId ) == JPH::EMotionType::Dynamic )
            {
                mSystem.GetBodyInterface().SetLinearVelocity( bodyId, ToJolt( velocity ) );
                mSystem.GetBodyInterface().SetAngularVelocity( bodyId, ToJolt( angular ) );
            }
        }
    }

    void RemoveBody( Entity entity )
    {
        erase_if( mPartOf, [entity]( const auto& entry ) { return entry.second == entity; } );
        const auto found = mBodies.find( entity );
        if ( found == mBodies.end() )
            return;
        if ( const auto vehicle = mVehicles.find( entity ); vehicle != mVehicles.end() and vehicle->second.mConstraint )
        {
            Unvehicle( vehicle->second );
            MarkConstraint( KeyOf( entity, Slot::Vehicle ) );
        }
        // Its joints go before it, made again with it, keeping their rest.
        for ( auto& [jointEntity, joint] : mJoints )
            if ( joint.mConstraint and ( joint.mFirst == entity or joint.mSecond == entity ) )
            {
                Unjoin( joint );
                MarkJoint( jointEntity, false );
            }
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        bodies.RemoveBody( found->second );
        bodies.DestroyBody( found->second );
        mBodies.erase( found );
    }

    // ---- ragdolls ----------------------------------------------------------------

    void Unragdoll( Entity entity, RagdollState& ragdoll )
    {
        for ( const JPH::Ref<JPH::TwoBodyConstraint>& joint : ragdoll.mJoints )
            if ( joint )
                mSystem.RemoveConstraint( joint );
        ragdoll.mJoints.clear();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const JPH::BodyID bodyId : ragdoll.mBodies )
        {
            bodies.RemoveBody( bodyId );
            bodies.DestroyBody( bodyId );
        }
        const vector<RagdollBone>& bones = ragdoll.mSkeleton.mBones;
        for ( u32 bone = 0; bone < ragdoll.mBodies.size(); ++bone )
            ForEachApart( entity, bones, bone, [&]( u64 a, u64 b ) { mPairs->Keep( a, b, false ); } );
        ragdoll.mBodies.clear();
    }

    void RemoveRagdolls()
    {
        for ( auto& [entity, ragdoll] : mRagdolls )
            Unragdoll( entity, ragdoll );
        mRagdolls.clear();
    }

    // The bones kept from colliding with `bone`: the one it swings from and
    // those that swing from the same.
    template <typename F>
    static void ForEachApart( Entity entity, const vector<RagdollBone>& bones, u32 bone, F&& fn )
    {
        const i32 parent = bones[bone].mParent;
        if ( parent < 0 )
            return;
        const u64 key = JointedPairs::KeyOf( entity, bone + 1 );
        fn( key, JointedPairs::KeyOf( entity, static_cast<u32>( parent ) + 1 ) );
        for ( u32 other = 0; other < bone; ++other )
            if ( bones[other].mParent == parent )
                fn( key, JointedPairs::KeyOf( entity, other + 1 ) );
    }

    void RagdollFailed( Entity entity, RagdollState& ragdoll, string_view why )
    {
        if ( ragdoll.mSaid )
            return;
        ragdoll.mSaid = true;
        LogError( "{}: ragdoll: {}", Describe( mWorld.GetScene(), entity ), why );
    }

    void BuildRagdoll( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        const auto ragdollMaybe = scene.Alive( entity ) ? scene.TryGet<Ragdoll>( entity ) : OptRef<Ragdoll>();
        if ( const auto found = mRagdolls.find( entity ); found != mRagdolls.end() )
        {
            const bool posed = not found->second.mBodies.empty();
            Unragdoll( entity, found->second );
            if ( not ragdollMaybe or not ragdollMaybe->mActive )
            {
                // Its animator poses it again, or it stands at rest.
                if ( posed and scene.Alive( entity ) and scene.Has<SkinPose>( entity ) )
                    scene.Remove<SkinPose>( entity );
                mRagdolls.erase( found );
                return;
            }
        }
        if ( not ragdollMaybe or not ragdollMaybe->mActive )
            return;
        const Ragdoll& settings = *ragdollMaybe;
        RagdollState& ragdoll = mRagdolls[entity];
        const auto modelMaybe = scene.TryGet<Model>( entity );
        if ( not modelMaybe )
            return RagdollFailed( entity, ragdoll, "no model - it goes on a skinned model" );
        auto readyMaybe = TryReadyModel( entity, modelMaybe->mAsset );
        if ( not readyMaybe )
            return RagdollFailed( entity, ragdoll, readyMaybe.error() );
        if ( not *readyMaybe )
            return;
        mWaiting.erase( entity );
        ragdoll.mModel = std::move( **readyMaybe );
        const ModelAsset& model = *ragdoll.mModel.TryGet();
        const vec3 scale = glm::abs( WorldScale( scene, entity ) );
        ragdoll.mScale = ( scale.x + scale.y + scale.z ) / 3.0f;
        ragdoll.mSkeleton = MakeRagdollSkeleton( model, settings, ragdoll.mScale );
        if ( ragdoll.mSkeleton.mBones.empty() )
            return RagdollFailed( entity, ragdoll, "its model has no skeleton" );
        ragdoll.mSaid = false;

        // Each bone where its joint is posed now.
        const Pose placed = PoseOf( ComputeWorldMatrix( scene, entity ) );
        const vector<mat4> poses = JointPoses( model, scene.TryGet<SkinPose>( entity ) );
        const vector<RagdollBone>& bones = ragdoll.mSkeleton.mBones;
        vector<mat4> bodyPoses;
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( u32 bone = 0; bone < bones.size(); ++bone )
        {
            const RagdollBone& made = bones[bone];
            const Pose joint = PoseOf( poses[made.mJoint] );
            const vec3 position = placed.mPosition + placed.mRotation * ( joint.mPosition * ragdoll.mScale );
            const quat rotation = glm::normalize( placed.mRotation * joint.mRotation );
            bodyPoses.push_back( glm::translate( mat4( 1.0f ), position ) * glm::mat4_cast( rotation ) );

            // A capsule from the joint to the tip, in the joint's space.
            const vec3 tip = made.mTip * ragdoll.mScale;
            const f32 radius = made.mRadius * ragdoll.mScale;
            const f32 halfCylinder = glm::length( tip ) * 0.5f - radius;
            JPH::RefConst<JPH::ShapeSettings> core;
            if ( halfCylinder > 0.0f )
                core = new JPH::CapsuleShapeSettings( halfCylinder, radius );
            else
                core = new JPH::SphereShapeSettings( max( glm::length( tip ) * 0.5f, radius ) );
            const JPH::ShapeSettings::ShapeResult shape =
                JPH::RotatedTranslatedShapeSettings(
                    ToJolt( tip * 0.5f ), JPH::Quat::sFromTo( JPH::Vec3::sAxisY(), ToJolt( glm::normalize( tip ) ) ),
                    core )
                    .Create();
            JPH::BodyCreationSettings body( shape.Get(), JPH::RVec3( ToJolt( position ) ), ToJolt( rotation ),
                                            JPH::EMotionType::Dynamic, Layers::cMoving );
            body.mUserData = UserDataOf( entity );
            body.mCollisionGroup =
                JPH::CollisionGroup( mPairs, static_cast<JPH::uint32>( UserDataOf( entity ) ), bone + 1 );
            body.mFriction = 0.6f;
            body.mLinearDamping = 0.05f;
            body.mAngularDamping = 0.3f;
            // A chain of joints kept within its limits as it lands takes more
            // of the solver than a lone body.
            body.mNumVelocityStepsOverride = 20;
            body.mNumPositionStepsOverride = 8;
            body.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            body.mMassPropertiesOverride.mMass = max( settings.mMass * made.mShare, 0.01f );
            const auto kept = mKeptBones.find( JointedPairs::KeyOf( entity, bone + 1 ) );
            JPH::Body* created =
                kept != mKeptBones.end() ? bodies.CreateBodyWithID( kept->second, body ) : bodies.CreateBody( body );
            if ( created == nullptr )
            {
                Unragdoll( entity, ragdoll );
                return RagdollFailed( entity, ragdoll, "no room for its bodies" );
            }
            bodies.AddBody( created->GetID(), JPH::EActivation::Activate );
            ragdoll.mBodies.push_back( created->GetID() );
        }

        // Each bone held to the one above at its joint, as they stand now.
        const JPH::BodyLockInterfaceNoLock& locks = mSystem.GetBodyLockInterfaceNoLock();
        ragdoll.mJoints.resize( bones.size() );
        for ( u32 bone = 0; bone < bones.size(); ++bone )
        {
            ForEachApart( entity, bones, bone, [&]( u64 a, u64 b ) { mPairs->Keep( a, b, true ); } );
            if ( bones[bone].mParent < 0 )
                continue;
            const auto parent = static_cast<u32>( bones[bone].mParent );
            BoneJoint joint = MakeBoneJoint( ragdoll.mSkeleton, bone, poses );
            // From the model's units to the bodies' metres.
            joint.mAbove.mPoint *= ragdoll.mScale;
            auto madeMaybe = TryMakeConstraint( joint.mJoint, *locks.TryGetBody( ragdoll.mBodies[parent] ),
                                                joint.mAbove, *locks.TryGetBody( ragdoll.mBodies[bone] ), joint.mOwn );
            if ( not madeMaybe )
                continue;
            ragdoll.mJoints[bone] = *madeMaybe;
            ragdoll.mJoints[bone]->SetUserData( KeyOf( entity, static_cast<Slot>( 2 + bone ) ) );
            mSystem.AddConstraint( ragdoll.mJoints[bone] );
        }
    }

    // Each active ragdoll's model drawn as its bones lie, after its animator
    // posed it.
    void PoseRagdolls()
    {
        Scene& scene = mWorld.GetScene();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( auto& [entity, ragdoll] : mRagdolls )
        {
            if ( ragdoll.mBodies.empty() or not scene.Alive( entity ) )
                continue;
            const ModelAsset& model = *ragdoll.mModel.TryGet();
            const Pose placed = PoseOf( ComputeWorldMatrix( scene, entity ) );
            const quat back = glm::inverse( placed.mRotation );
            ragdoll.mPoses.resize( model.mJoints.size() );
            for ( u32 bone = 0; bone < ragdoll.mBodies.size(); ++bone )
            {
                JPH::RVec3 position;
                JPH::Quat rotation;
                bodies.GetPositionAndRotation( ragdoll.mBodies[bone], position, rotation );
                const vec3 at = back * ( FromJolt( JPH::Vec3( position ) ) - placed.mPosition ) / ragdoll.mScale;
                ragdoll.mPoses[ragdoll.mSkeleton.mBones[bone].mJoint] =
                    glm::translate( mat4( 1.0f ), at ) * glm::mat4_cast( back * FromJolt( rotation ) );
            }
            const auto skinMaybe = scene.TryGet<SkinPose>( entity );
            SkinPose& skin = skinMaybe ? *skinMaybe : scene.Add<SkinPose>( entity );
            PoseSkin( model, ragdoll.mSkeleton, ragdoll.mPoses, skin );
        }
    }

    // ---- vehicles ----------------------------------------------------------------

    // Its constraint out of the system, and its wheels' children back on
    // their own.
    void Unvehicle( VehicleState& vehicle )
    {
        if ( vehicle.mConstraint )
        {
            mSystem.RemoveStepListener( vehicle.mConstraint );
            mSystem.RemoveConstraint( vehicle.mConstraint );
            vehicle.mConstraint = nullptr;
        }
        Scene& scene = mWorld.GetScene();
        for ( const auto& [wheel, place] : vehicle.mWheels )
        {
            if ( scene.Alive( wheel ) )
                for ( const Entity child : ChildrenOf( scene, wheel ) )
                    if ( scene.Has<ParentOffset>( child ) )
                        scene.Remove<ParentOffset>( child );
            mWheelOf.erase( wheel );
        }
        vehicle.mWheels.clear();
    }

    void RemoveVehicles()
    {
        for ( auto& [entity, vehicle] : mVehicles )
            Unvehicle( vehicle );
        mVehicles.clear();
        mWheelOf.clear();
    }

    void VehicleFailed( Entity entity, VehicleState& vehicle, string_view why )
    {
        if ( vehicle.mSaid or not mWaiting.empty() )
            return;
        vehicle.mSaid = true;
        LogError( "{}: vehicle: {}", Describe( mWorld.GetScene(), entity ), why );
    }

    void BuildVehicle( Entity entity )
    {
        const Scene& scene = mWorld.GetScene();
        const auto vehicleMaybe = scene.Alive( entity ) ? scene.TryGet<Vehicle>( entity ) : OptRef<const Vehicle>();
        if ( not vehicleMaybe )
        {
            if ( const auto found = mVehicles.find( entity ); found != mVehicles.end() )
            {
                Unvehicle( found->second );
                mVehicles.erase( found );
            }
            return;
        }
        VehicleState& state = mVehicles[entity];
        Unvehicle( state );
        const auto body = mBodies.find( entity );
        if ( body == mBodies.end() or
             mSystem.GetBodyInterface().GetMotionType( body->second ) != JPH::EMotionType::Dynamic )
            return VehicleFailed( entity, state, "no body that moves - it goes on a rigid_body with colliders" );
        vector<Entity> wheels;
        CollectWheels( scene, entity, wheels );
        if ( wheels.empty() )
            return VehicleFailed( entity, state, "no wheels - entities under it with a vehicle_wheel" );
        vector<WheelPlace> places;
        for ( const Entity wheel : wheels )
        {
            const WheelPlace place{ .mWheel = scene.Get<VehicleWheel>( wheel ),
                                    .mPosition = PoseOf( PlacementIn( scene, entity, wheel ) ).mPosition };
            places.push_back( place );
            state.mWheels.emplace_back( wheel, place );
            mWheelOf[wheel] = entity;
        }
        JPH::Body& made = *mSystem.GetBodyLockInterfaceNoLock().TryGetBody( body->second );
        state.mConstraint = MakeVehicle( *vehicleMaybe, places, made, Layers::cMoving );
        state.mConstraint->SetUserData( KeyOf( entity, Slot::Vehicle ) );
        state.mSaid = false;
        mSystem.AddConstraint( state.mConstraint );
        mSystem.AddStepListener( state.mConstraint );
    }

    // The drivers' hands and feet, before the step.
    void SteerVehicles()
    {
        const Scene& scene = mWorld.GetScene();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const Entity entity : scene.EntitiesWith<Vehicle>() )
        {
            const auto found = mVehicles.find( entity );
            if ( found == mVehicles.end() or not found->second.mConstraint )
                continue;
            const Vehicle& vehicle = scene.Get<Vehicle>( entity );
            Steer( vehicle, *found->second.mConstraint );
            // A driver's touch wakes a vehicle that went to sleep.
            if ( vehicle.mThrottle != 0.0f or vehicle.mSteer != 0.0f )
                bodies.ActivateBody( mBodies.at( entity ) );
        }
    }

    // Each wheel's children where the wheel is now: turned, steered, sprung.
    void PlaceWheels()
    {
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<Vehicle>() )
        {
            const auto found = mVehicles.find( entity );
            if ( found == mVehicles.end() or not found->second.mConstraint )
                continue;
            const VehicleState& vehicle = found->second;
            for ( u32 index = 0; index < vehicle.mWheels.size(); ++index )
            {
                const auto& [wheel, place] = vehicle.mWheels[index];
                if ( not scene.Alive( wheel ) )
                    continue;
                const mat4 offset = WheelOffset( *vehicle.mConstraint, index, place );
                for ( const Entity child : ChildrenOf( scene, wheel ) )
                    if ( const auto offsetMaybe = scene.TryGet<ParentOffset>( child ) )
                        offsetMaybe->mMatrix = offset;
                    else
                        scene.Add<ParentOffset>( child, ParentOffset{ offset } );
            }
        }
    }

    // ---- joints ------------------------------------------------------------------

    // `fresh`: its fields changed, so its rest is taken again from how the
    // bodies stand.
    void MarkJoint( Entity entity, bool fresh )
    {
        if ( fresh )
            if ( const auto found = mJoints.find( entity ); found != mJoints.end() )
            {
                found->second.mConnectedFrame.reset();
                found->second.mSaid = false;
            }
        MarkConstraint( KeyOf( entity, Slot::Joint ) );
    }

    // The motor as the component has it now, on a joint made.
    void Drive( Entity entity )
    {
        const auto found = mJoints.find( entity );
        const auto jointMaybe = mWorld.GetScene().TryGet<Joint>( entity );
        if ( found == mJoints.end() or not found->second.mConstraint or not jointMaybe )
        {
            MarkJoint( entity, false );
            return;
        }
        ApplyMotor( *jointMaybe, *found->second.mConstraint );
        Wake( found->second );
    }

    void Wake( const JointState& joint )
    {
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const Entity entity : { joint.mFirst, joint.mSecond } )
            if ( const auto found = mBodies.find( entity ); found != mBodies.end() )
                bodies.ActivateBody( found->second );
    }

    void Unjoin( JointState& joint )
    {
        if ( not joint.mConstraint )
            return;
        mSystem.RemoveConstraint( joint.mConstraint );
        joint.mConstraint = nullptr;
        Apart( joint, false );
    }

    // The two bodies kept from colliding while the joint holds, unless it
    // says they collide.
    void Apart( const JointState& joint, bool apart )
    {
        if ( joint.mSecond == Entity::Null or not joint.mApart )
            return;
        mPairs->Keep( JointedPairs::KeyOf( joint.mFirst ), JointedPairs::KeyOf( joint.mSecond ), apart );
    }

    void MarkConstraint( u64 key )
    {
        if ( mDirtyConstraintSet.insert( key ).second )
            mDirtyConstraints.push_back( key );
    }

    // Joints and vehicles: those a snapshot kept first, in Jolt's order
    // then, as it restores each by its place.
    void BuildConstraints()
    {
        if ( mDirtyConstraints.empty() )
            return;
        const auto keptEnd = stable_partition( mDirtyConstraints.begin(), mDirtyConstraints.end(),
                                               [this]( u64 key ) { return mKeptOrder.contains( key ); } );
        sort( mDirtyConstraints.begin(), keptEnd,
              [this]( u64 a, u64 b ) { return mKeptOrder.at( a ) < mKeptOrder.at( b ); } );
        const vector<u64> dirty = std::exchange( mDirtyConstraints, {} );
        mDirtyConstraintSet.clear();
        // A ragdoll is made whole, at the first of its keys.
        hset<Entity> ragdolls;
        for ( const u64 key : dirty )
            if ( SlotOf( key ) == Slot::Joint )
                BuildJoint( EntityOf( key ) );
            else if ( SlotOf( key ) == Slot::Vehicle )
                BuildVehicle( EntityOf( key ) );
            else if ( ragdolls.insert( EntityOf( key ) ).second )
                BuildRagdoll( EntityOf( key ) );
    }

    // The body an entity is part of, or none.
    opt<JPH::BodyID> TryBodyOf( Entity entity, Entity& owner )
    {
        owner = BodyOwnerOf( mWorld.GetScene(), entity );
        const auto found = mBodies.find( owner );
        if ( found == mBodies.end() )
            return nullopt;
        return found->second;
    }

    // Said once, until the joint changes; nothing said while a model loads.
    void JointFailed( Entity entity, JointState& joint, string_view why )
    {
        if ( joint.mSaid or not mWaiting.empty() )
            return;
        joint.mSaid = true;
        LogError( "{}: joint: {}", Describe( mWorld.GetScene(), entity ), why );
    }

    void BuildJoint( Entity entity )
    {
        const Scene& scene = mWorld.GetScene();
        const auto jointMaybe = scene.Alive( entity ) ? scene.TryGet<Joint>( entity ) : OptRef<const Joint>();
        if ( not jointMaybe )
        {
            if ( const auto found = mJoints.find( entity ); found != mJoints.end() )
            {
                Unjoin( found->second );
                mJoints.erase( found );
            }
            return;
        }
        const Joint& joint = *jointMaybe;
        JointState& state = mJoints[entity];
        Unjoin( state );

        Entity first;
        const auto firstMaybe = TryBodyOf( entity, first );
        if ( not firstMaybe )
            return JointFailed( entity, state, "no body to hold - a collider is missing" );
        Entity second = Entity::Null;
        opt<JPH::BodyID> secondMaybe;
        Entity target = Entity::Null;
        if ( not joint.mConnected.mPath.Empty() )
        {
            const auto targetMaybe = TryResolve( scene, joint.mConnected, entity );
            if ( not targetMaybe )
                return JointFailed( entity, state, "connected: " + targetMaybe.error() );
            target = *targetMaybe;
            secondMaybe = TryBodyOf( target, second );
            if ( not secondMaybe )
                return JointFailed( entity, state,
                                    format( "connected: {} has no body - a collider is missing",
                                            PathOf( scene, target ).mPath.String() ) );
            if ( *secondMaybe == *firstMaybe )
                return JointFailed( entity, state, "connected: the body it is part of already" );
        }

        const JPH::BodyLockInterfaceNoLock& locks = mSystem.GetBodyLockInterfaceNoLock();
        JPH::Body& firstBody = *locks.TryGetBody( *firstMaybe );
        JPH::Body& secondBody = secondMaybe ? *locks.TryGetBody( *secondMaybe ) : JPH::Body::sFixedToWorld;
        const auto poseOf = []( const JPH::Body& body ) {
            if ( &body == &JPH::Body::sFixedToWorld )
                return mat4( 1.0f );
            return glm::translate( mat4( 1.0f ), FromJolt( JPH::Vec3( body.GetPosition() ) ) ) *
                   glm::mat4_cast( FromJolt( body.GetRotation() ) );
        };

        // On its own body, as the scene places it there.
        const vec3 axis = glm::length( joint.mAxis ) > 0.0f ? glm::normalize( joint.mAxis ) : vec3( 1.0f, 0.0f, 0.0f );
        const JointFrame own{ joint.mAnchor, axis, FromJolt( ToJolt( axis ).GetNormalizedPerpendicular() ) };
        const JointFrame inFirst = Transformed( PlacementIn( scene, first, entity ), own );
        // On the other, as they stand when it is first made; kept after.
        if ( const auto kept = mKeptFrames.find( entity ); kept != mKeptFrames.end() )
            state.mConnectedFrame = kept->second;
        if ( not state.mConnectedFrame )
        {
            JointFrame inSecond =
                Transformed( glm::inverse( poseOf( secondBody ) ), Transformed( poseOf( firstBody ), inFirst ) );
            if ( joint.mKind == "distance"_n )
                inSecond.mPoint =
                    target == Entity::Null
                        ? joint.mConnectedAnchor
                        : vec3( PlacementIn( scene, second, target ) * vec4( joint.mConnectedAnchor, 1.0f ) );
            state.mConnectedFrame = inSecond;
        }

        // The entity's body second: a slider's position, a hinge's angle are
        // how far it went from what it is held to.
        auto madeMaybe = TryMakeConstraint( joint, secondBody, *state.mConnectedFrame, firstBody, inFirst );
        if ( not madeMaybe )
            return JointFailed( entity, state, madeMaybe.error() );
        state.mConstraint = *madeMaybe;
        state.mConstraint->SetUserData( KeyOf( entity, Slot::Joint ) );
        state.mFirst = first;
        state.mSecond = second;
        state.mApart = not joint.mCollide;
        state.mSaid = false;
        mSystem.AddConstraint( state.mConstraint );
        Apart( state, true );
        Wake( state );
    }

    // Joints that held more than they bear give way, and their scripts hear.
    void Break( f32 dt )
    {
        const Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<Joint>() )
        {
            const f32 most = scene.Get<Joint>( entity ).mBreakForce;
            const auto found = mJoints.find( entity );
            if ( most <= 0.0f or found == mJoints.end() or not found->second.mConstraint or
                 not found->second.mConstraint->GetEnabled() )
                continue;
            if ( HeldForce( *found->second.mConstraint, dt ) <= most )
                continue;
            found->second.mConstraint->SetEnabled( false );
            Wake( found->second );
            mWorld.GetEvents().Emit( ScriptCall{ entity, "on_joint_break"_n, {} } );
        }
    }

    // The collider's shape at `scale`; nothing while a mesh's model loads.
    expected<opt<JPH::RefConst<JPH::Shape>>, string> TryShape( Entity entity, vec3 scale )
    {
        const Scene& scene = mWorld.GetScene();
        scale = glm::abs( scale );
        vector<string> colliders;
        opt<JPH::ShapeSettings::ShapeResult> made;
        if ( const auto boxMaybe = scene.TryGet<BoxCollider>( entity ) )
        {
            colliders.push_back( "box_collider" );
            const vec3 half = boxMaybe->mSize * scale * 0.5f;
            const f32 smallest = min( { half.x, half.y, half.z } );
            if ( smallest <= 0.0f )
                return unexpected( "box_collider.size: every side above 0"s );
            made = JPH::BoxShapeSettings( ToJolt( half ), min( JPH::cDefaultConvexRadius, smallest * 0.5f ) ).Create();
        }
        if ( const auto sphereMaybe = scene.TryGet<SphereCollider>( entity ) )
        {
            colliders.push_back( "sphere_collider" );
            const f32 radius = sphereMaybe->mRadius * max( { scale.x, scale.y, scale.z } );
            if ( radius <= 0.0f )
                return unexpected( "sphere_collider.radius: above 0"s );
            if ( not made )
                made = JPH::SphereShapeSettings( radius ).Create();
        }
        if ( const auto capsuleMaybe = scene.TryGet<CapsuleCollider>( entity ) )
        {
            colliders.push_back( "capsule_collider" );
            if ( not made )
            {
                auto capsuleShapeMaybe = TryCapsule( capsuleMaybe->mRadius * max( scale.x, scale.z ),
                                                     capsuleMaybe->mHeight * scale.y, 0.0f );
                if ( not capsuleShapeMaybe )
                    return unexpected( "capsule_collider: " + capsuleShapeMaybe.error() );
                made = std::move( *capsuleShapeMaybe );
            }
        }
        if ( const auto convexMaybe = scene.TryGet<ConvexCollider>( entity ) )
        {
            colliders.push_back( "convex_collider" );
            if ( not made )
            {
                auto convexShapeMaybe = TryConvex( entity, convexMaybe->mModel, scale );
                if ( not convexShapeMaybe )
                    return unexpected( "convex_collider: " + convexShapeMaybe.error() );
                if ( not *convexShapeMaybe )
                    return opt<JPH::RefConst<JPH::Shape>>();
                made = std::move( **convexShapeMaybe );
            }
        }
        if ( const auto meshMaybe = scene.TryGet<MeshCollider>( entity ) )
        {
            colliders.push_back( "mesh_collider" );
            if ( not made )
            {
                auto meshShapeMaybe = TryMesh( entity, meshMaybe->mModel, scale );
                if ( not meshShapeMaybe )
                    return unexpected( "mesh_collider: " + meshShapeMaybe.error() );
                if ( not *meshShapeMaybe )
                    return opt<JPH::RefConst<JPH::Shape>>();
                made = std::move( **meshShapeMaybe );
            }
        }
        if ( colliders.size() > 1 )
        {
            string names;
            for ( const string& name : colliders )
                names += ( names.empty() ? "" : ", " ) + name;
            LogError( "{}: one collider an entity, it has {}: the {} counts", Describe( scene, entity ), names,
                      colliders.front() );
        }
        if ( not made )
            return opt<JPH::RefConst<JPH::Shape>>();
        if ( made->HasError() )
            return unexpected( string( made->GetError().c_str() ) );
        return opt<JPH::RefConst<JPH::Shape>>( made->Get() );
    }

    // Upright, `height` from end to end, its centre `lift` above the origin;
    // a sphere when too short for a cylinder between the caps.
    static expected<JPH::ShapeSettings::ShapeResult, string> TryCapsule( f32 radius, f32 height, f32 lift )
    {
        if ( radius <= 0.0f or height <= 0.0f )
            return unexpected( "radius and height above 0"s );
        const f32 halfCylinder = height * 0.5f - radius;
        JPH::RefConst<JPH::ShapeSettings> core;
        if ( halfCylinder > 0.0f )
            core = new JPH::CapsuleShapeSettings( halfCylinder, radius );
        else
            core = new JPH::SphereShapeSettings( radius );
        if ( lift == 0.0f )
            return core->Create();
        return JPH::RotatedTranslatedShapeSettings( JPH::Vec3( 0.0f, lift, 0.0f ), JPH::Quat::sIdentity(), core )
            .Create();
    }

    // The model a collider is made of, loaded; nothing while it loads - the
    // entity is built again when it is in.
    expected<opt<AssetRef<ModelAsset>>, string> TryReadyModel( Entity entity, const AssetId& modelId )
    {
        if ( not modelId )
            return unexpected( "no model chosen"s );
        AssetRef<ModelAsset> modelRef = mAssets.Load<ModelAsset>( modelId );
        if ( modelRef.State() == AssetState::Loading )
        {
            mWaiting[entity] = std::move( modelRef );
            return opt<AssetRef<ModelAsset>>();
        }
        if ( not modelRef.TryGet() )
            return unexpected( modelRef.Entry().Error() );
        return opt<AssetRef<ModelAsset>>( std::move( modelRef ) );
    }

    // The hull of the model's points, scaled; nothing while it loads.
    expected<opt<JPH::ShapeSettings::ShapeResult>, string> TryConvex( Entity entity,
                                                                      const AssetId& modelId,
                                                                      vec3 scale )
    {
        auto readyMaybe = TryReadyModel( entity, modelId );
        if ( not readyMaybe )
            return unexpected( std::move( readyMaybe.error() ) );
        if ( not *readyMaybe )
            return opt<JPH::ShapeSettings::ShapeResult>();
        const AssetRef<ModelAsset>& modelRef = **readyMaybe;
        JPH::Array<JPH::Vec3> points;
        for ( const ModelMesh& mesh : modelRef.TryGet()->mMeshes )
            for ( const ModelVertex& vertex : mesh.mVertices )
                points.push_back( ToJolt( vertex.mPosition * scale ) );
        if ( points.size() < 4 )
            return unexpected( format( "{} has too few points to wrap", modelRef.Entry().Path().String() ) );
        return opt<JPH::ShapeSettings::ShapeResult>( JPH::ConvexHullShapeSettings( points ).Create() );
    }

    // The model's triangles, scaled; nothing while it loads.
    expected<opt<JPH::ShapeSettings::ShapeResult>, string> TryMesh( Entity entity, const AssetId& modelId, vec3 scale )
    {
        auto readyMaybe = TryReadyModel( entity, modelId );
        if ( not readyMaybe )
            return unexpected( std::move( readyMaybe.error() ) );
        if ( not *readyMaybe )
            return opt<JPH::ShapeSettings::ShapeResult>();
        const AssetRef<ModelAsset>& modelRef = **readyMaybe;
        const auto modelMaybe = modelRef.TryGet();
        JPH::VertexList vertices;
        JPH::IndexedTriangleList triangles;
        for ( const ModelMesh& mesh : modelMaybe->mMeshes )
        {
            const auto base = static_cast<JPH::uint32>( vertices.size() );
            for ( const ModelVertex& vertex : mesh.mVertices )
            {
                const vec3 at = vertex.mPosition * scale;
                vertices.push_back( JPH::Float3( at.x, at.y, at.z ) );
            }
            if ( mesh.mLods.empty() )
                continue;
            const vector<u32>& indices = mesh.mLods.front().mIndices;
            for ( size_t i = 0; i + 2 < indices.size(); i += 3 )
                triangles.push_back(
                    JPH::IndexedTriangle( base + indices[i], base + indices[i + 1], base + indices[i + 2], 0 ) );
        }
        if ( triangles.empty() )
            return unexpected( format( "{} has no triangles", modelRef.Entry().Path().String() ) );
        return opt<JPH::ShapeSettings::ShapeResult>(
            JPH::MeshShapeSettings( std::move( vertices ), std::move( triangles ) ).Create() );
    }

    // False when there is nothing to make: no collider, a model loading.
    expected<bool, string> TryMakeBody( Entity entity )
    {
        const Scene& scene = mWorld.GetScene();
        const Pose pose = PoseOf( ComputeWorldMatrix( scene, entity ) );
        const auto bodyMaybe = scene.TryGet<RigidBody>( entity );
        const auto isMesh = [&scene]( Entity part ) {
            return scene.Has<MeshCollider>( part ) and not scene.Has<BoxCollider>( part ) and
                   not scene.Has<SphereCollider>( part ) and not scene.Has<CapsuleCollider>( part ) and
                   not scene.Has<ConvexCollider>( part );
        };

        // The colliders that make the body: its own, and with a rigid_body
        // those under it.
        vector<Entity> parts;
        if ( HasCollider( scene, entity ) )
            parts.push_back( entity );
        if ( bodyMaybe )
            CollectParts( scene, entity, parts );
        if ( parts.empty() )
            return false;
        // A body of meshes alone stands still; a moving one leaves them out.
        const bool meshesAlone = ranges::all_of( parts, isMesh );
        JPH::EMotionType motion = JPH::EMotionType::Static;
        if ( bodyMaybe and not meshesAlone )
            motion = bodyMaybe->mKinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        else if ( bodyMaybe )
            LogError( "{}: a mesh_collider never moves: its rigid_body is not heard - a convex_collider moves",
                      Describe( scene, entity ) );

        JPH::StaticCompoundShapeSettings compound;
        opt<JPH::RefConst<JPH::Shape>> single;
        u32 used = 0;
        bool waiting = false;
        for ( const Entity part : parts )
        {
            if ( motion != JPH::EMotionType::Static and isMesh( part ) )
            {
                LogError( "{}: a mesh_collider never moves: left out of {}'s body - a convex_collider moves",
                          Describe( scene, part ), Describe( scene, entity ) );
                continue;
            }
            auto shapeMaybe = TryShape( part, WorldScale( scene, part ) );
            if ( not shapeMaybe )
            {
                if ( part == entity )
                    return unexpected( std::move( shapeMaybe.error() ) );
                LogError( "{}: {}", Describe( scene, part ), shapeMaybe.error() );
                continue;
            }
            if ( not *shapeMaybe )
            {
                waiting = true;
                continue;
            }
            const Pose placed = PoseOf( PlacementIn( scene, entity, part ) );
            compound.AddShape( ToJolt( placed.mPosition ), ToJolt( placed.mRotation ), **shapeMaybe );
            if ( part == entity )
                single = *shapeMaybe;
            ++used;
        }
        // Whole or not yet: a body is made when every part's model is in.
        if ( waiting or used == 0 )
            return false;
        for ( const Entity part : parts )
            mPartOf[part] = entity;
        JPH::RefConst<JPH::Shape> shape;
        if ( used == 1 and single )
            shape = *single;
        else
        {
            const JPH::ShapeSettings::ShapeResult madeCompound = compound.Create();
            if ( madeCompound.HasError() )
                return unexpected( string( madeCompound.GetError().c_str() ) );
            shape = madeCompound.Get();
        }

        if ( const auto vehicleMaybe = scene.TryGet<Vehicle>( entity );
             vehicleMaybe and vehicleMaybe->mMassOffset != vec3( 0.0f ) )
        {
            const JPH::ShapeSettings::ShapeResult offset =
                JPH::OffsetCenterOfMassShapeSettings( ToJolt( vehicleMaybe->mMassOffset ), shape ).Create();
            if ( offset.HasError() )
                return unexpected( string( offset.GetError().c_str() ) );
            shape = offset.Get();
        }

        JPH::BodyCreationSettings settings( shape, JPH::RVec3( ToJolt( pose.mPosition ) ), ToJolt( pose.mRotation ),
                                            motion,
                                            motion == JPH::EMotionType::Static ? Layers::cStatic : Layers::cMoving );
        settings.mUserData = UserDataOf( entity );
        settings.mCollisionGroup = JPH::CollisionGroup( mPairs, static_cast<JPH::uint32>( UserDataOf( entity ) ), 0 );
        if ( bodyMaybe )
        {
            settings.mFriction = bodyMaybe->mFriction;
            settings.mRestitution = bodyMaybe->mRestitution;
            settings.mLinearDamping = bodyMaybe->mLinearDamping;
            settings.mAngularDamping = bodyMaybe->mAngularDamping;
            settings.mGravityFactor = bodyMaybe->mGravityScale;
            if ( motion == JPH::EMotionType::Dynamic )
            {
                if ( bodyMaybe->mMass <= 0.0f )
                    return unexpected( "rigid_body.mass: above 0"s );
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = bodyMaybe->mMass;
            }
        }

        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        const auto kept = mKeptIds.find( entity );
        JPH::Body* body =
            kept != mKeptIds.end() ? bodies.CreateBodyWithID( kept->second, settings ) : bodies.CreateBody( settings );
        if ( body == nullptr )
            return unexpected( "no room for another body, or its id is taken"s );
        bodies.AddBody( body->GetID(), motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate
                                                                          : JPH::EActivation::Activate );
        mBodies[entity] = body->GetID();
        return true;
    }

    void MakeCharacter( Entity entity, const CharacterController& controller )
    {
        const Scene& scene = mWorld.GetScene();
        const Pose pose = PoseOf( ComputeWorldMatrix( scene, entity ) );
        auto shapeMaybe = TryCapsule( controller.mRadius, controller.mHeight, controller.mHeight * 0.5f );
        if ( not shapeMaybe or shapeMaybe->HasError() )
        {
            LogError( "{}: character_controller: {}", Describe( scene, entity ),
                      shapeMaybe ? string( shapeMaybe->GetError().c_str() ) : shapeMaybe.error() );
            return;
        }
        JPH::CharacterVirtualSettings settings;
        settings.mShape = shapeMaybe->Get();
        settings.mMaxSlopeAngle = controller.mMaxSlope;
        settings.mMass = controller.mMass;
        settings.mUp = JPH::Vec3::sAxisY();
        // What touches its lower round end is ground; the side, wall.
        settings.mSupportingVolume = JPH::Plane( JPH::Vec3::sAxisY(), -controller.mRadius );
        // Upright whatever the transform says: a character does not tip over.
        mCharacters[entity] = new JPH::CharacterVirtual( &settings, JPH::RVec3( ToJolt( pose.mPosition ) ),
                                                         JPH::Quat::sIdentity(), UserDataOf( entity ), &mSystem );
        mStepHeights[entity] = controller.mStepHeight;
    }

    // A transform set from outside - a script, the editor - moves the body
    // there at once; a kinematic one goes there in its next step.
    void Teleport( Entity entity ) { Teleport( entity, entity ); }

    // `moved`: the entity whose transform was set, which `entity` is or is
    // under. A part moved within its body makes the body again; one moved
    // with it is where it was in it.
    void Teleport( Entity entity, Entity moved )
    {
        if ( mDirtySet.contains( entity ) )
            return;
        const Scene& scene = mWorld.GetScene();
        if ( const auto part = mPartOf.find( entity ); part != mPartOf.end() and part->second != entity )
        {
            if ( part->second != moved and not IsAncestor( scene, moved, part->second ) )
                MarkDirty( part->second );
        }
        const Pose pose = PoseOf( ComputeWorldMatrix( scene, entity ) );
        if ( const auto found = mBodies.find( entity ); found != mBodies.end() )
        {
            JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
            if ( bodies.GetMotionType( found->second ) != JPH::EMotionType::Kinematic )
                bodies.SetPositionAndRotation( found->second, JPH::RVec3( ToJolt( pose.mPosition ) ),
                                               ToJolt( pose.mRotation ), JPH::EActivation::Activate );
        }
        if ( const auto found = mCharacters.find( entity ); found != mCharacters.end() )
            found->second->SetPosition( JPH::RVec3( ToJolt( pose.mPosition ) ) );
        // The children's bodies are where their parent took them.
        for ( const Entity child : ChildrenOf( scene, entity ) )
            Teleport( child, moved );
    }

    opt<JPH::BodyID> TryMovingBody( Entity entity )
    {
        BuildDirty();
        const auto found = mBodies.find( entity );
        if ( found == mBodies.end() or
             mSystem.GetBodyInterface().GetMotionType( found->second ) == JPH::EMotionType::Static )
            return nullopt;
        return found->second;
    }

    void Step( f32 dt )
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Physics" );
        // Meshes whose models came in - or failed, which is said once.
        vector<Entity> failed;
        for ( const auto& [entity, modelRef] : mWaiting )
            if ( modelRef.State() == AssetState::Ready )
                MarkDirty( entity );
            else if ( modelRef.State() == AssetState::Failed )
            {
                LogError( "{}: collider: {}", Describe( mWorld.GetScene(), entity ), modelRef.Entry().Error() );
                failed.push_back( entity );
            }
        for ( const Entity entity : failed )
            mWaiting.erase( entity );
        BuildDirty();

        Scene& scene = mWorld.GetScene();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        // Kinematic bodies go where their transforms are, carrying what they
        // touch on the way.
        for ( const Entity entity : scene.EntitiesWith<RigidBody>() )
            if ( const auto found = mBodies.find( entity );
                 found != mBodies.end() and bodies.GetMotionType( found->second ) == JPH::EMotionType::Kinematic )
            {
                const Pose pose = PoseOf( ComputeWorldMatrix( scene, entity ) );
                bodies.MoveKinematic( found->second, JPH::RVec3( ToJolt( pose.mPosition ) ), ToJolt( pose.mRotation ),
                                      dt );
            }

        // Characters on what the bodies were at the step's start.
        for ( const Entity entity : scene.EntitiesWith<CharacterController>() )
            if ( const auto found = mCharacters.find( entity ); found != mCharacters.end() )
            {
                JPH::CharacterVirtual::ExtendedUpdateSettings settings;
                settings.mWalkStairsStepUp = JPH::Vec3( 0.0f, mStepHeights[entity], 0.0f );
                found->second->ExtendedUpdate( dt, mSystem.GetGravity(), settings,
                                               mSystem.GetDefaultBroadPhaseLayerFilter( Layers::cMoving ),
                                               mSystem.GetDefaultLayerFilter( Layers::cMoving ), JPH::BodyFilter(),
                                               JPH::ShapeFilter(), mTempAllocator );
            }

        SteerVehicles();
        (void)mContacts.Take();
        const JPH::EPhysicsUpdateError error = mSystem.Update( dt, 1, &mTempAllocator, &mJobs );
        if ( error != JPH::EPhysicsUpdateError::None )
            LogWarning( "physics: the step ran out of room ({}): raise the limits", static_cast<u32>( error ) );

        Break( dt );
        WriteBack();
        PlaceWheels();
        TellContacts();
    }

    // What the step moved, into the entities' transforms - written as data
    // is, not set: nobody needs telling, and the bodies are where it says.
    void WriteBack()
    {
        Scene& scene = mWorld.GetScene();
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const Entity entity : scene.EntitiesWith<RigidBody>() )
        {
            const auto found = mBodies.find( entity );
            const auto transformMaybe = scene.TryGet<Transform>( entity );
            if ( found == mBodies.end() or not transformMaybe or
                 bodies.GetMotionType( found->second ) != JPH::EMotionType::Dynamic )
                continue;
            JPH::RVec3 position;
            JPH::Quat rotation;
            bodies.GetPositionAndRotation( found->second, position, rotation );
            Place( entity, *transformMaybe, FromJolt( JPH::Vec3( position ) ), FromJolt( rotation ), true );
        }
        for ( const Entity entity : scene.EntitiesWith<CharacterController>() )
        {
            const auto found = mCharacters.find( entity );
            const auto transformMaybe = scene.TryGet<Transform>( entity );
            if ( found == mCharacters.end() or not transformMaybe )
                continue;
            Place( entity, *transformMaybe, FromJolt( JPH::Vec3( found->second->GetPosition() ) ), quat(), false );
        }
    }

    // A world pose into the entity's transform, relative to its parent.
    void Place( Entity entity, Transform& transform, vec3 position, const quat& rotation, bool turn )
    {
        const mat4 toLocal = glm::inverse( ParentMatrix( mWorld.GetScene(), entity ) );
        transform.mPosition = vec3( toLocal * vec4( position, 1.0f ) );
        if ( turn )
            transform.mRotation = AnglesOf( glm::normalize( glm::quat_cast( mat3( toLocal ) ) * rotation ) );
    }

    // The step's new contacts, to both entities' scripts: the other, where
    // they touched, and the other's surface there, facing the one told.
    void TellContacts()
    {
        const Scene& scene = mWorld.GetScene();
        for ( const Contact& contact : mContacts.Take() )
        {
            const Entity first = EntityOf( contact.mFirst );
            const Entity second = EntityOf( contact.mSecond );
            if ( not scene.Alive( first ) or not scene.Alive( second ) )
                continue;
            mWorld.GetEvents().Emit(
                ScriptCall{ first, "on_collision"_n, { second, contact.mPoint, -contact.mNormal } } );
            mWorld.GetEvents().Emit(
                ScriptCall{ second, "on_collision"_n, { first, contact.mPoint, contact.mNormal } } );
        }
    }

    World& mWorld;
    AssetRegistry& mAssets;
    // Jolt's allocator and types, before anything of Jolt is made.
    JoltStarted mJolt;
    // What the system is made with outlives it.
    BroadPhaseLayers mBroadPhaseLayers;
    LayerVsBroadPhase mLayerVsBroadPhase;
    LayerPairs mLayerPairs;
    ContactLog mContacts;
    JPH::TempAllocatorImpl mTempAllocator{ 16 * 1024 * 1024 };
    // One thread: the same every time, and what a replay repeats. Jobs'
    // pool comes later.
    JPH::JobSystemSingleThreaded mJobs{ JPH::cMaxPhysicsJobs };
    JPH::PhysicsSystem mSystem;
    hmap<Entity, JPH::BodyID> mBodies;
    hmap<Entity, JPH::Ref<JPH::CharacterVirtual>> mCharacters;
    hmap<Entity, f32> mStepHeights;
    // Colliders whose models load.
    hmap<Entity, AssetRef<ModelAsset>> mWaiting;
    // The colliders that are parts of a body, and whose: a part's change or
    // end makes its body again.
    hmap<Entity, Entity> mPartOf;
    JPH::Ref<JointedPairs> mPairs = new JointedPairs();
    hmap<Entity, JointState> mJoints;
    hmap<Entity, VehicleState> mVehicles;
    hmap<Entity, RagdollState> mRagdolls;
    // From a snapshot being restored: the ids of ragdolls' bones' bodies.
    hmap<u64, JPH::BodyID> mKeptBones;
    // Each wheel's vehicle: a wheel gone finds it here.
    hmap<Entity, Entity> mWheelOf;
    bool mBuilding = false;
    // Joints and vehicles to make again, by KeyOf.
    vector<u64> mDirtyConstraints;
    hset<u64> mDirtyConstraintSet;
    // From a snapshot being restored: each joint's place in Jolt's order and
    // its rest on the connected body.
    hmap<u64, u32> mKeptOrder;
    hmap<Entity, JointFrame> mKeptFrames;
    vector<Entity> mDirty;
    hset<Entity> mDirtySet;
    // From a snapshot being restored: Jolt's state, the bodies' ids.
    opt<string> mRestoringState;
    hmap<Entity, JPH::BodyID> mKeptIds;
};

// ---- what scripts get ---------------------------------------------------------

PhysicsWorld& PhysicsOf( World& world )
{
    return world.Get<PhysicsWorld>();
}

[[noreturn]] void NoBody( World& world, Entity entity, string_view what )
{
    throw LuaError( format( "{}: {} with nothing to move - a collider is missing, or it stands still",
                            Describe( world.GetScene(), entity ), what ) );
}

void ScriptsOfPhysics( ScriptApi& api )
{
    api.ExtendComponent(
        Name::Declare( "rigid_body" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            type.Property(
                    "velocity",
                    [&world]( ScriptComponent& self ) {
                        const auto velocityMaybe = PhysicsOf( world ).TryVelocity( self.mEntity );
                        if ( not velocityMaybe )
                            NoBody( world, self.mEntity, "rigid_body.velocity" );
                        return *velocityMaybe;
                    },
                    [&world]( ScriptComponent& self, vec3 velocity ) {
                        if ( not PhysicsOf( world ).TrySetVelocity( self.mEntity, velocity ) )
                            NoBody( world, self.mEntity, "rigid_body.velocity" );
                    } )
                .Property(
                    "angular_velocity",
                    [&world]( ScriptComponent& self ) {
                        const auto velocityMaybe = PhysicsOf( world ).TryAngularVelocity( self.mEntity );
                        if ( not velocityMaybe )
                            NoBody( world, self.mEntity, "rigid_body.angular_velocity" );
                        return *velocityMaybe;
                    },
                    [&world]( ScriptComponent& self, vec3 velocity ) {
                        if ( not PhysicsOf( world ).TrySetAngularVelocity( self.mEntity, velocity ) )
                            NoBody( world, self.mEntity, "rigid_body.angular_velocity" );
                    } )
                .Method( "apply_impulse",
                         [&world]( ScriptComponent& self, vec3 impulse ) {
                             if ( not PhysicsOf( world ).TryAddImpulse( self.mEntity, impulse ) )
                                 NoBody( world, self.mEntity, "rigid_body:apply_impulse" );
                         } )
                .Method( "add_force", [&world]( ScriptComponent& self, vec3 force ) {
                    if ( not PhysicsOf( world ).TryAddForce( self.mEntity, force ) )
                        NoBody( world, self.mEntity, "rigid_body:add_force" );
                } );
        },
        "    velocity: vector\n"
        "    angular_velocity: vector\n"
        "    function apply_impulse(self, impulse: vector): ()\n"
        "    function add_force(self, force: vector): ()\n" );

    api.ExtendComponent(
        Name::Declare( "character_controller" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            const auto state = [&world]( Entity entity ) {
                const auto stateMaybe = PhysicsOf( world ).TryCharacter( entity );
                if ( not stateMaybe )
                    throw LuaError( format( "{}: character_controller not made - its shape is wrong",
                                            Describe( world.GetScene(), entity ) ) );
                return *stateMaybe;
            };
            type.Property(
                    "velocity", [state]( ScriptComponent& self ) { return state( self.mEntity ).mVelocity; },
                    [&world]( ScriptComponent& self, vec3 velocity ) {
                        (void)PhysicsOf( world ).TrySetCharacterVelocity( self.mEntity, velocity );
                    } )
                .Property( "on_ground", [state]( ScriptComponent& self ) { return state( self.mEntity ).mOnGround; } )
                .Property( "ground_normal",
                           [state]( ScriptComponent& self ) { return state( self.mEntity ).mGroundNormal; } )
                .Property( "ground_velocity",
                           [state]( ScriptComponent& self ) { return state( self.mEntity ).mGroundVelocity; } );
        },
        "    velocity: vector\n"
        "    on_ground: boolean\n"
        "    ground_normal: vector\n"
        "    ground_velocity: vector\n" );

    api.AddGlobals(
        []( LuaState& lua, World& world ) {
            LuaTable physics = lua.NewTable();
            physics["raycast"] = [&lua, &world]( vec3 from, vec3 to ) {
                const auto hitMaybe = PhysicsOf( world ).TryRaycast( from, to );
                if ( not hitMaybe )
                    return LuaValue();
                LuaTable hit = lua.NewTable();
                hit["entity"] = hitMaybe->mEntity;
                hit["point"] = hitMaybe->mPoint;
                hit["normal"] = hitMaybe->mNormal;
                hit["distance"] = static_cast<f64>( hitMaybe->mDistance );
                return LuaValue( hit );
            };
            physics["overlap_sphere"] = [&lua, &world]( vec3 centre, f32 radius ) {
                LuaTable touched = lua.NewTable();
                for ( const Entity entity : PhysicsOf( world ).OverlapSphere( centre, radius ) )
                    touched.Append( lua.Value( entity ) );
                return touched;
            };
            physics["gravity"] = [&world]() { return PhysicsOf( world ).Gravity(); };
            lua.Globals()["physics"] = physics;
        },
        "declare physics: {\n"
        "    raycast: (from: vector, to: vector) -> { entity: entity, point: vector, normal: vector, distance: number "
        "}?,\n"
        "    overlap_sphere: (centre: vector, radius: number) -> { entity },\n"
        "    gravity: () -> vector,\n"
        "}\n" );

    api.AddCallback( "on_collision", "on_collision( self, other: entity, point: vector, normal: vector )" );

    api.ExtendComponent(
        Name::Declare( "joint" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            type.Property( "broken", [&world]( ScriptComponent& self ) {
                return PhysicsOf( world ).TryJointBroken( self.mEntity ).value_or( false );
            } );
        },
        "    broken: boolean\n" );
    api.AddCallback( "on_joint_break", "on_joint_break( self )" );

    api.ExtendComponent(
        Name::Declare( "ragdoll" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            type.Method( "push", [&world]( ScriptComponent& self, vec3 impulse ) {
                if ( not PhysicsOf( world ).TryPushRagdoll( self.mEntity, impulse ) )
                    throw LuaError( format( "{}: ragdoll:push while it is not active",
                                            Describe( world.GetScene(), self.mEntity ) ) );
            } );
        },
        "    function push(self, impulse: vector): ()\n" );

    api.ExtendComponent(
        Name::Declare( "vehicle" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            const auto gauges = [&world]( Entity entity ) {
                return PhysicsOf( world ).TryVehicle( entity ).value_or( VehicleGauges{} );
            };
            type.Property( "speed", [gauges]( ScriptComponent& self ) { return gauges( self.mEntity ).mSpeed; } )
                .Property( "rpm", [gauges]( ScriptComponent& self ) { return gauges( self.mEntity ).mRpm; } )
                .Property( "gear", [gauges]( ScriptComponent& self ) { return gauges( self.mEntity ).mGear; } );
        },
        "    speed: number\n"
        "    rpm: number\n"
        "    gear: number\n" );
}
}

ModuleType PhysicsModule( AssetRegistry& assets )
{
    return ModuleType{
        .mName = "physics",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<BoxCollider>( "box_collider" ).Field<&BoxCollider::mSize>( "size" );
                reflection.Type<SphereCollider>( "sphere_collider" ).Field<&SphereCollider::mRadius>( "radius" );
                reflection.Type<CapsuleCollider>( "capsule_collider" )
                    .Field<&CapsuleCollider::mRadius>( "radius" )
                    .Field<&CapsuleCollider::mHeight>( "height" );
                reflection.Type<ConvexCollider>( "convex_collider" )
                    .Field<&ConvexCollider::mModel>( "model", { .mAssetKind = cModelKind.data() } );
                reflection.Type<MeshCollider>( "mesh_collider" )
                    .Field<&MeshCollider::mModel>( "model", { .mAssetKind = cModelKind.data() } );
                reflection.Type<RigidBody>( "rigid_body" )
                    .Field<&RigidBody::mMass>( "mass" )
                    .Field<&RigidBody::mFriction>( "friction", { .mMin = 0, .mMax = 1, .mFlags = FieldInfo::Slider } )
                    .Field<&RigidBody::mRestitution>( "restitution",
                                                      { .mMin = 0, .mMax = 1, .mFlags = FieldInfo::Slider } )
                    .Field<&RigidBody::mLinearDamping>( "linear_damping" )
                    .Field<&RigidBody::mAngularDamping>( "angular_damping" )
                    .Field<&RigidBody::mGravityScale>( "gravity_scale" )
                    .Field<&RigidBody::mKinematic>( "kinematic" );
                reflection.Type<CharacterController>( "character_controller" )
                    .Field<&CharacterController::mRadius>( "radius" )
                    .Field<&CharacterController::mHeight>( "height" )
                    .Field<&CharacterController::mMaxSlope>( "max_slope",
                                                             { .mMin = 0, .mMax = 90, .mFlags = FieldInfo::Angle } )
                    .Field<&CharacterController::mStepHeight>( "step_height" )
                    .Field<&CharacterController::mMass>( "mass" );
                reflection.Type<Vehicle>( "vehicle" )
                    .Field<&Vehicle::mEngineTorque>( "engine_torque", { .mTooltip = "Newton-metres at the engine" } )
                    .Field<&Vehicle::mMaxRpm>( "max_rpm" )
                    .Field<&Vehicle::mMassOffset>( "mass_offset",
                                                   { .mTooltip = "Where its weight is, from its origin: lower rolls "
                                                                 "over less" } )
                    .Field<&Vehicle::mMaxTilt>( "max_tilt", { .mMin = 0, .mMax = 180, .mFlags = FieldInfo::Angle } )
                    .Field<&Vehicle::mThrottle>( "throttle", { .mMin = -1, .mMax = 1, .mFlags = FieldInfo::Slider } )
                    .Field<&Vehicle::mSteer>( "steer", { .mMin = -1, .mMax = 1, .mFlags = FieldInfo::Slider } )
                    .Field<&Vehicle::mBrake>( "brake", { .mMin = 0, .mMax = 1, .mFlags = FieldInfo::Slider } )
                    .Field<&Vehicle::mHandbrake>( "handbrake", { .mMin = 0, .mMax = 1, .mFlags = FieldInfo::Slider } );
                reflection.Type<VehicleWheel>( "vehicle_wheel" )
                    .Field<&VehicleWheel::mRadius>( "radius" )
                    .Field<&VehicleWheel::mWidth>( "width" )
                    .Field<&VehicleWheel::mSuspension>( "suspension",
                                                        { .mTooltip = "How far it rises from where it is placed" } )
                    .Field<&VehicleWheel::mStiffness>(
                        "stiffness", { .mTooltip = "How many times a second the suspension springs" } )
                    .Field<&VehicleWheel::mDamping>( "damping", { .mMin = 0, .mMax = 2, .mFlags = FieldInfo::Slider } )
                    .Field<&VehicleWheel::mMaxSteer>( "max_steer",
                                                      { .mMin = 0, .mMax = 80, .mFlags = FieldInfo::Angle } )
                    .Field<&VehicleWheel::mDrive>( "drive" )
                    .Field<&VehicleWheel::mHandbrake>( "handbrake" )
                    .Field<&VehicleWheel::mBrakeTorque>( "brake_torque" )
                    .Field<&VehicleWheel::mGrip>( "grip", { .mMin = 0, .mMax = 3, .mFlags = FieldInfo::Slider } );
                reflection.Type<Ragdoll>( "ragdoll" )
                    .Field<&Ragdoll::mActive>( "active", { .mTooltip = "On, the bones fall as bodies" } )
                    .Field<&Ragdoll::mMass>( "mass" )
                    .Field<&Ragdoll::mThickness>( "thickness",
                                                  { .mMin = 0.05f, .mMax = 0.6f, .mFlags = FieldInfo::Slider } )
                    .Field<&Ragdoll::mMinBone>( "min_bone",
                                                { .mTooltip = "Metres: a shorter bone rides on the one above" } )
                    .Field<&Ragdoll::mSwing>( "swing",
                                              { .mMin = 0,
                                                .mMax = 180,
                                                .mFlags = FieldInfo::Angle,
                                                .mTooltip = "A bone of a name it does not know: how far it swings" } )
                    .Field<&Ragdoll::mTwist>( "twist",
                                              { .mMin = 0,
                                                .mMax = 180,
                                                .mFlags = FieldInfo::Angle,
                                                .mTooltip = "A bone of a name it does not know: how far it twists" } );
                reflection.Type<Joint>( "joint" )
                    .Field<&Joint::mKind>( "kind", { .mChoices = "fixed,hinge,slider,distance,swing_twist" } )
                    .Field<&Joint::mConnected>(
                        "connected", { .mTooltip = "What it holds the entity to; none: the world where it is" } )
                    .Field<&Joint::mAnchor>( "anchor", { .mTooltip = "Where, in the entity's own space" } )
                    .Field<&Joint::mAxis>( "axis", { .mTooltip = "A hinge's axis, a slider's line, a twist's axis" } )
                    .Field<&Joint::mConnectedAnchor>(
                        "connected_anchor", { .mTooltip = "distance: the other end, in the connected entity's space" } )
                    .Field<&Joint::mMin>( "min", { .mTooltip = "The lower limit: radians, or metres; equal: none" } )
                    .Field<&Joint::mMax>( "max", { .mTooltip = "The upper limit: radians, or metres; equal: none" } )
                    .Field<&Joint::mSwing>( "swing", { .mMin = 0, .mMax = 180, .mFlags = FieldInfo::Angle } )
                    .Field<&Joint::mSpring>( "spring",
                                             { .mTooltip = "How many times a second a limit springs back; 0 rigid" } )
                    .Field<&Joint::mDamping>( "damping", { .mMin = 0, .mMax = 2, .mFlags = FieldInfo::Slider } )
                    .Field<&Joint::mMotor>( "motor", { .mChoices = "off,velocity,position" } )
                    .Field<&Joint::mTarget>( "target", { .mTooltip = "A velocity, or a position from the rest" } )
                    .Field<&Joint::mMaxForce>( "max_force" )
                    .Field<&Joint::mBreakForce>( "break_force", { .mTooltip = "Newtons it bears; 0 never breaks" } )
                    .Field<&Joint::mCollide>( "collide" );
                componentTypes.Add<BoxCollider>();
                componentTypes.Add<SphereCollider>();
                componentTypes.Add<CapsuleCollider>();
                componentTypes.Add<ConvexCollider>();
                componentTypes.Add<MeshCollider>();
                componentTypes.Add<RigidBody>();
                componentTypes.Add<CharacterController>();
                componentTypes.Add<Joint>();
                componentTypes.Add<Ragdoll>();
                componentTypes.Add<Vehicle>();
                componentTypes.Add<VehicleWheel>();
            },
        .mCreate = [&assets]( World& world ) -> Scope<Module> { return CreateScope<Physics>( world, assets ); },
        .mScripts = &ScriptsOfPhysics,
    };
}
}
