#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include <cstdarg>
#include <cstdio>
// clang-format off
// Jolt.h comes first: it sets up what the rest of Jolt is built on.
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/RegisterTypes.h>

namespace bubble
{
namespace
{
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

// ---- between glm and Jolt ---------------------------------------------------

JPH::Vec3 ToJolt( vec3 v )
{
    return JPH::Vec3( v.x, v.y, v.z );
}

JPH::Quat ToJolt( const quat& q )
{
    return JPH::Quat( q.x, q.y, q.z, q.w );
}

vec3 FromJolt( JPH::Vec3Arg v )
{
    return vec3( v.GetX(), v.GetY(), v.GetZ() );
}

quat FromJolt( JPH::QuatArg q )
{
    return quat( q.GetW(), q.GetX(), q.GetY(), q.GetZ() );
}

Entity EntityOf( JPH::uint64 userData )
{
    return Entity( static_cast<Entity::entity_type>( userData ) );
}

JPH::uint64 UserDataOf( Entity entity )
{
    return static_cast<JPH::uint64>( static_cast<Entity::entity_type>( entity ) );
}

// A world matrix as a body takes it: where, which way, how large.
struct Pose
{
    vec3 mPosition{ 0.0f };
    quat mRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 mScale{ 1.0f };
};

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

// The bodies' state as a snapshot keeps it: Jolt's own bytes, and which
// entity had which body, to make them again under their ids.
class PhysicsSnapshot final : public ModuleSnapshot
{
public:
    PhysicsSnapshot( string state, vector<pair<Entity, JPH::uint32>> bodies )
        : mState( std::move( state ) ),
          mBodies( std::move( bodies ) )
    {
    }

    const string& State() const { return mState; }
    const vector<pair<Entity, JPH::uint32>>& Bodies() const { return mBodies; }

private:
    string mState;
    vector<pair<Entity, JPH::uint32>> mBodies;
};

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
        Watch<MeshCollider>( { "model"_n }, rebuild );
        Watch<RigidBody>( { "mass"_n, "friction"_n, "restitution"_n, "linear_damping"_n, "angular_damping"_n,
                            "gravity_scale"_n, "kinematic"_n },
                          rebuild );
        Watch<CharacterController>( { "radius"_n, "height"_n, "max_slope"_n, "step_height"_n }, rebuild );
        world.OnAdded<Transform>( rebuild );
        world.OnRemoved<Transform>( rebuild );
        world.OnChanged<Transform>( "scale"_n, rebuild );
        const auto teleport = [this]( Entity entity ) { Teleport( entity ); };
        world.OnChanged<Transform>( "position"_n, teleport );
        world.OnChanged<Transform>( "rotation"_n, teleport );

        world.AddStep( Phase::Physics, [this]( f32 dt ) { Step( dt ); } );
        world.GetServices().Add<PhysicsWorld>( *this );
    }

    ~Physics() override
    {
        // Characters first: they hold the system.
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
        return CreateRef<PhysicsSnapshot>( recorder.GetData(), std::move( bodies ) );
    }

    void Restoring( OptRef<const ModuleSnapshot> snapshotMaybe ) override
    {
        // Every body goes now, so none holds an id a kept one comes back
        // under; the scene's components coming back mark what to make.
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        for ( const auto& [entity, bodyId] : mBodies )
        {
            bodies.RemoveBody( bodyId );
            bodies.DestroyBody( bodyId );
        }
        mBodies.clear();
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
    }

    void BuildDirty()
    {
        if ( mDirty.empty() )
            return;
        // Those with ids kept by a snapshot first, in the order their ids
        // were given - the order they were made in -, and no new body may
        // take one of their ids.
        const auto keptEnd = stable_partition( mDirty.begin(), mDirty.end(),
                                               [this]( Entity entity ) { return mKeptIds.contains( entity ); } );
        sort( mDirty.begin(), keptEnd,
              [this]( Entity a, Entity b ) { return mKeptIds.at( a ).GetIndex() < mKeptIds.at( b ).GetIndex(); } );
        const vector<Entity> dirty = std::exchange( mDirty, {} );
        mDirtySet.clear();
        for ( const Entity entity : dirty )
            Build( entity );
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
        const auto found = mBodies.find( entity );
        if ( found == mBodies.end() )
            return;
        JPH::BodyInterface& bodies = mSystem.GetBodyInterface();
        bodies.RemoveBody( found->second );
        bodies.DestroyBody( found->second );
        mBodies.erase( found );
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

    // The model's triangles, scaled; nothing while it loads.
    expected<opt<JPH::ShapeSettings::ShapeResult>, string> TryMesh( Entity entity, const AssetId& modelId, vec3 scale )
    {
        if ( not modelId )
            return unexpected( "no model chosen"s );
        AssetRef<ModelAsset> modelRef = mAssets.Load<ModelAsset>( modelId );
        if ( modelRef.State() == AssetState::Loading )
        {
            mWaiting[entity] = std::move( modelRef );
            return opt<JPH::ShapeSettings::ShapeResult>();
        }
        const auto modelMaybe = modelRef.TryGet();
        if ( not modelMaybe )
            return unexpected( modelRef.Entry().Error() );
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
        auto shapeMaybe = TryShape( entity, WorldScale( scene, entity ) );
        if ( not shapeMaybe )
            return unexpected( std::move( shapeMaybe.error() ) );
        if ( not *shapeMaybe )
            return false;

        const auto bodyMaybe = scene.TryGet<RigidBody>( entity );
        const bool mesh = scene.Has<MeshCollider>( entity ) and not scene.Has<BoxCollider>( entity ) and
                          not scene.Has<SphereCollider>( entity ) and not scene.Has<CapsuleCollider>( entity );
        JPH::EMotionType motion = JPH::EMotionType::Static;
        if ( bodyMaybe and not mesh )
            motion = bodyMaybe->mKinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        else if ( bodyMaybe )
            LogError( "{}: a mesh_collider never moves: its rigid_body is not heard", Describe( scene, entity ) );

        JPH::BodyCreationSettings settings( **shapeMaybe, JPH::RVec3( ToJolt( pose.mPosition ) ),
                                            ToJolt( pose.mRotation ), motion,
                                            motion == JPH::EMotionType::Static ? Layers::cStatic : Layers::cMoving );
        settings.mUserData = UserDataOf( entity );
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
    void Teleport( Entity entity )
    {
        if ( mDirtySet.contains( entity ) )
            return;
        const Scene& scene = mWorld.GetScene();
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
            Teleport( child );
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
                LogError( "{}: mesh_collider: {}", Describe( mWorld.GetScene(), entity ), modelRef.Entry().Error() );
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

        (void)mContacts.Take();
        const JPH::EPhysicsUpdateError error = mSystem.Update( dt, 1, &mTempAllocator, &mJobs );
        if ( error != JPH::EPhysicsUpdateError::None )
            LogWarning( "physics: the step ran out of room ({}): raise the limits", static_cast<u32>( error ) );

        WriteBack();
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
    // Mesh colliders whose models load.
    hmap<Entity, AssetRef<ModelAsset>> mWaiting;
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
                reflection.Type<MeshCollider>( "mesh_collider" ).Field<&MeshCollider::mModel>( "model" );
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
                    .Field<&CharacterController::mStepHeight>( "step_height" );
                componentTypes.Add<BoxCollider>();
                componentTypes.Add<SphereCollider>();
                componentTypes.Add<CapsuleCollider>();
                componentTypes.Add<MeshCollider>();
                componentTypes.Add<RigidBody>();
                componentTypes.Add<CharacterController>();
            },
        .mCreate = [&assets]( World& world ) -> Scope<Module> { return CreateScope<Physics>( world, assets ); },
        .mScripts = &ScriptsOfPhysics,
    };
}
}
