#include "bubble/animation/animation.hpp"
#include "bubble/animation/animation_graph.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/core/jobs.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/variant.hpp"
#include "bubble/world/app.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"
#include "inertialization.hpp"
#include "ozz_assets.hpp"
#include <cstring>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/track_sampling_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

namespace bubble
{
namespace
{
// A parameter's value as scripts give it.
using AnimationValue = variant<f32, bool>;

using SoaPose = vector<ozz::math::SoaTransform>;

// Float4x4 is four columns of four floats, as mat4 is: the same bytes, SSE
// or not.
static_assert( sizeof( ozz::math::Float4x4 ) == sizeof( mat4 ) );

mat4 ToMat4( const ozz::math::Float4x4& matrix )
{
    mat4 result;
    std::memcpy( &result, &matrix, sizeof( result ) );
    return result;
}

template <typename T>
ozz::span<T> Span( vector<T>& items )
{
    return ozz::span<T>( items.data(), items.size() );
}

template <typename T>
ozz::span<const T> Span( const vector<T>& items )
{
    return ozz::span<const T>( items.data(), items.size() );
}

// ozz keeps four joints in a SoaTransform, a component across the four
// lanes; the easing wants a struct a joint.
void SoaToJoints( const SoaPose& soa, vector<JointPose>& joints )
{
    for ( size_t j = 0; j < joints.size(); ++j )
    {
        const ozz::math::SoaTransform& t = soa[j / 4];
        const size_t lane = j % 4;
        array<array<f32, 4>, 10> lanes{};
        ozz::math::StorePtrU( t.translation.x, lanes[0].data() );
        ozz::math::StorePtrU( t.translation.y, lanes[1].data() );
        ozz::math::StorePtrU( t.translation.z, lanes[2].data() );
        ozz::math::StorePtrU( t.rotation.x, lanes[3].data() );
        ozz::math::StorePtrU( t.rotation.y, lanes[4].data() );
        ozz::math::StorePtrU( t.rotation.z, lanes[5].data() );
        ozz::math::StorePtrU( t.rotation.w, lanes[6].data() );
        ozz::math::StorePtrU( t.scale.x, lanes[7].data() );
        ozz::math::StorePtrU( t.scale.y, lanes[8].data() );
        ozz::math::StorePtrU( t.scale.z, lanes[9].data() );
        joints[j].mTranslation = vec3( lanes[0][lane], lanes[1][lane], lanes[2][lane] );
        joints[j].mRotation = quat( lanes[6][lane], lanes[3][lane], lanes[4][lane], lanes[5][lane] );
        joints[j].mScale = vec3( lanes[7][lane], lanes[8][lane], lanes[9][lane] );
    }
}

void JointsToSoa( const vector<JointPose>& joints, SoaPose& soa )
{
    for ( size_t s = 0; s < soa.size(); ++s )
    {
        // Lanes past the last joint keep the identity, as ozz pads them.
        array<JointPose, 4> lanes{};
        for ( size_t lane = 0; lane < 4; ++lane )
            if ( s * 4 + lane < joints.size() )
                lanes[lane] = joints[s * 4 + lane];
        const auto load = [&]( auto component ) {
            return ozz::math::simd_float4::Load( component( lanes[0] ), component( lanes[1] ), component( lanes[2] ),
                                                 component( lanes[3] ) );
        };
        soa[s].translation = { load( []( const JointPose& p ) { return p.mTranslation.x; } ),
                               load( []( const JointPose& p ) { return p.mTranslation.y; } ),
                               load( []( const JointPose& p ) { return p.mTranslation.z; } ) };
        soa[s].rotation = { load( []( const JointPose& p ) { return p.mRotation.x; } ),
                            load( []( const JointPose& p ) { return p.mRotation.y; } ),
                            load( []( const JointPose& p ) { return p.mRotation.z; } ),
                            load( []( const JointPose& p ) { return p.mRotation.w; } ) };
        soa[s].scale = { load( []( const JointPose& p ) { return p.mScale.x; } ),
                         load( []( const JointPose& p ) { return p.mScale.y; } ),
                         load( []( const JointPose& p ) { return p.mScale.z; } ) };
    }
}

// What a frame's pose is made in - ozz's buffers -, and the poses the last
// frames showed, which an eased transition starts from.
struct PoseBuffers
{
    explicit PoseBuffers( const ozz::animation::Skeleton& skeleton )
    {
        const auto soaJoints = static_cast<size_t>( skeleton.num_soa_joints() );
        const auto joints = static_cast<size_t>( skeleton.num_joints() );
        for ( size_t slot = 0; slot < mContexts.size(); ++slot )
        {
            mContexts[slot] = CreateScope<ozz::animation::SamplingJob::Context>( skeleton.num_joints() );
            mSampled[slot].resize( soaJoints );
        }
        mLocals.resize( soaJoints );
        mModels.resize( joints );
        mJoints.resize( joints );
        mPrevious.resize( joints );
        mBeforePrevious.resize( joints );
    }

    // A blend plays two clips at most; a context remembers where in its
    // clip it sampled last.
    array<Scope<ozz::animation::SamplingJob::Context>, 2> mContexts;
    array<SoaPose, 2> mSampled;
    SoaPose mLocals;
    vector<ozz::math::Float4x4> mModels;
    vector<JointPose> mJoints;
    vector<JointPose> mPrevious;
    vector<JointPose> mBeforePrevious;
    u32 mHistory = 0;
    f32 mLastDt = 0;
    Inertializer mInertializer;
    // The graph state's mEntered the pose last saw.
    u32 mSeenEntered = 0;
    bool mSampledOnce = false;
};

struct PendingSet
{
    string mParameter;
    AnimationValue mValue;
    bool mTrigger = false;
};

// An animator's runtime: what it plays, and where it is.
struct AnimatorRuntime
{
    AssetRef<AnimationGraph> mGraph;
    AssetId mModelId;
    AssetRef<ModelAsset> mModel;
    AssetRef<SkeletonAsset> mSkeleton;
    AssetRef<ClipsAsset> mClips;
    // The versions of the graph, model, skeleton and clips it was made from:
    // a reload makes it again.
    array<u32, 4> mVersions{};
    bool mReady = false;
    AnimationGraphState mState;
    // Set before the graph came.
    vector<PendingSet> mPending;
    // By state, the clip of each of its points - one for a clip -; nothing for
    // one the model lacks.
    vector<vector<opt<u32>>> mClipOf;
    Scope<PoseBuffers> mPose;
    // What the last step's root motion asked for, in the world.
    AnimationRootMotion mRootMotion;
    // What was logged last, so it is not again and again.
    string mLogged;
};

// One animator as a snapshot keeps it.
struct AnimatorKept
{
    Entity mEntity;
    AnimationGraphState mState;
    AnimationRootMotion mRootMotion;
};

class AnimationSnapshot final : public ModuleSnapshot
{
public:
    vector<AnimatorKept> mAnimators;
};

// Where a clip's root went from one phase to another, in the model's space:
// across the ground, and the turn about the up axis in radians; around the
// seam when the phase wrapped.
pair<vec3, f32> RootTravel( const ClipAsset& clip, f32 from, f32 to, bool wrapped )
{
    const auto sample = [&]( f32 ratio ) {
        ozz::math::Float3 position;
        ozz::animation::Float3TrackSamplingJob positionJob;
        positionJob.track = clip.mRootTranslation.get();
        positionJob.ratio = ratio;
        positionJob.result = &position;
        positionJob.Run();
        ozz::math::Quaternion rotation;
        ozz::animation::QuaternionTrackSamplingJob rotationJob;
        rotationJob.track = clip.mRootRotation.get();
        rotationJob.ratio = ratio;
        rotationJob.result = &rotation;
        rotationJob.Run();
        // The track turns about the up axis alone.
        return pair<vec3, f32>( vec3( position.x, position.y, position.z ), 2.0f * atan2( rotation.y, rotation.w ) );
    };
    const auto between = [&]( f32 a, f32 b ) {
        const auto [atA, turnA] = sample( a );
        const auto [atB, turnB] = sample( b );
        // The short way round: past a half turn the angles wrap.
        const f32 turn = turnB - turnA;
        const f32 full = 2.0f * glm::pi<f32>();
        return pair<vec3, f32>( atB - atA, turn - full * round( turn / full ) );
    };
    if ( not wrapped )
        return between( from, to );
    // Forwards past the end, or backwards past the start.
    const auto first = to < from ? between( from, 1.0f ) : between( from, 0.0f );
    const auto second = to < from ? between( 0.0f, to ) : between( 1.0f, to );
    return { first.first + second.first, first.second + second.second };
}

string Names( const AnimationGraph& graph )
{
    string names;
    for ( const AnimationParameter& parameter : graph.mParameters )
        names += ( names.empty() ? "" : ", " ) + parameter.mName;
    return names.empty() ? "none" : names;
}

string DescribeValue( const AnimationValue& value )
{
    if ( const bool* flag = get_if<bool>( &value ) )
        return *flag ? "true" : "false";
    return format( "{}", get<f32>( value ) );
}

class Animation final : public Module, public AnimationWorld
{
public:
    Animation( World& world, AssetRegistry& assets ) : mWorld( world ), mAssets( assets )
    {
        world.OnAdded<Animator>( [this]( Entity entity ) { Build( entity ); } );
        world.OnChanged<Animator>( "graph"_n, [this]( Entity entity ) { Build( entity ); } );
        world.OnRemoved<Animator>( [this]( Entity entity ) {
            Scene& scene = mWorld.GetScene();
            scene.Remove<AnimatorRuntime>( entity );
            scene.Remove<SkinPose>( entity );
        } );
        world.OnRemoved<BoneAttachment>( [this]( Entity entity ) {
            mWorld.GetScene().Remove<ParentOffset>( entity );
            mAttachmentLogged.erase( entity );
        } );
        world.AddStep( Phase::Physics, [this]( f32 dt ) { Step( dt ); } );
        world.AddStep( Phase::Animation, [this]( f32 dt ) { Pose( dt ); } );
        world.GetServices().Add<AnimationWorld>( *this );
    }

    Ref<const ModuleSnapshot> TrySnapshot() override
    {
        auto snapshot = CreateRef<AnimationSnapshot>();
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<AnimatorRuntime>() )
        {
            const AnimatorRuntime& runtime = scene.Get<AnimatorRuntime>( entity );
            snapshot->mAnimators.push_back(
                AnimatorKept{ .mEntity = entity, .mState = runtime.mState, .mRootMotion = runtime.mRootMotion } );
        }
        return snapshot;
    }

    void Restoring( OptRef<const ModuleSnapshot> snapshotMaybe ) override
    {
        mRestoring.clear();
        if ( snapshotMaybe )
            mRestoring = static_cast<const AnimationSnapshot&>( *snapshotMaybe ).mAnimators;
        // What the animators play stays loaded across: the ones made again
        // find it in memory and play from the first step, as the originals
        // did.
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<AnimatorRuntime>() )
        {
            const AnimatorRuntime& runtime = scene.Get<AnimatorRuntime>( entity );
            mHeld.push_back( Held{ runtime.mGraph, runtime.mModel, runtime.mSkeleton, runtime.mClips } );
        }
    }

    void Restored() override
    {
        Scene& scene = mWorld.GetScene();
        for ( const AnimatorKept& kept : mRestoring )
            if ( const auto runtimeMaybe = scene.TryGet<AnimatorRuntime>( kept.mEntity ) )
            {
                runtimeMaybe->mState = kept.mState;
                runtimeMaybe->mRootMotion = kept.mRootMotion;
                runtimeMaybe->mPending.clear();
            }
        mRestoring.clear();
        for ( const Entity entity : scene.EntitiesWith<AnimatorRuntime>() )
            Prepare( entity, scene.Get<AnimatorRuntime>( entity ) );
        mHeld.clear();
    }

    expected<void, string> TrySet( Entity entity, string_view parameter, f32 value ) override
    {
        return TrySetValue( entity, parameter, AnimationValue( value ), false );
    }

    expected<void, string> TrySet( Entity entity, string_view parameter, bool value ) override
    {
        return TrySetValue( entity, parameter, AnimationValue( value ), false );
    }

    expected<void, string> TrySetValue( Entity entity, string_view parameter, AnimationValue value, bool trigger )
    {
        const auto runtimeMaybe = mWorld.GetScene().TryGet<AnimatorRuntime>( entity );
        if ( not runtimeMaybe )
            return unexpected( format( "{} has no animator", Describe( mWorld.GetScene(), entity ) ) );
        AnimatorRuntime& runtime = *runtimeMaybe;
        const auto graphMaybe = runtime.mGraph.TryGet();
        if ( not graphMaybe )
        {
            runtime.mPending.push_back(
                PendingSet{ .mParameter = string( parameter ), .mValue = value, .mTrigger = trigger } );
            return {};
        }
        StartState( runtime, *graphMaybe );
        return TryApply( runtime, *graphMaybe, parameter, value, trigger );
    }

    expected<void, string> TryTrigger( Entity entity, string_view trigger ) override
    {
        return TrySetValue( entity, trigger, AnimationValue( true ), true );
    }

    opt<string> TryState( Entity entity ) const override
    {
        const auto runtimeMaybe = mWorld.GetScene().TryGet<AnimatorRuntime>( entity );
        if ( not runtimeMaybe or runtimeMaybe->mState.mState < 0 )
            return nullopt;
        const auto graphMaybe = runtimeMaybe->mGraph.TryGet();
        if ( not graphMaybe or static_cast<size_t>( runtimeMaybe->mState.mState ) >= graphMaybe->mStates.size() )
            return nullopt;
        return graphMaybe->mStates[static_cast<size_t>( runtimeMaybe->mState.mState )].mName;
    }

    AnimationRootMotion RootMotion( Entity entity ) const override
    {
        const auto runtimeMaybe = mWorld.GetScene().TryGet<AnimatorRuntime>( entity );
        return runtimeMaybe ? runtimeMaybe->mRootMotion : AnimationRootMotion{};
    }

    opt<mat4> TryJoint( Entity entity, string_view joint ) const override
    {
        const auto runtimeMaybe = mWorld.GetScene().TryGet<AnimatorRuntime>( entity );
        if ( not runtimeMaybe or not runtimeMaybe->mReady or not runtimeMaybe->mPose or
             not runtimeMaybe->mPose->mSampledOnce )
            return nullopt;
        const auto jointMaybe = TryJointOf( *runtimeMaybe->mSkeleton.TryGet()->mSkeleton, joint );
        if ( not jointMaybe )
            return nullopt;
        return ToMat4( runtimeMaybe->mPose->mModels[*jointMaybe] );
    }

private:
    static opt<u32> TryJointOf( const ozz::animation::Skeleton& skeleton, string_view name )
    {
        const auto names = skeleton.joint_names();
        for ( u32 j = 0; j < names.size(); ++j )
            if ( names[j] == name )
                return j;
        return nullopt;
    }

    // The asset of the entity's model component, read through reflection:
    // the render module's, which this one does not know.
    AssetId ModelOf( Entity entity ) const
    {
        const auto modelTypeMaybe = mWorld.GetApp().GetComponentTypes().TryFind( "model"_n );
        if ( not modelTypeMaybe )
            return {};
        const MetaAny component = modelTypeMaybe->mGet( mWorld.GetScene(), entity );
        if ( not component )
            return {};
        const auto assetMaybe = mWorld.GetApp().GetReflection().TryGet( component, "asset"_n );
        if ( not assetMaybe )
            return {};
        const AssetId* assetId = static_cast<const MetaAny&>( *assetMaybe ).try_cast<const AssetId>();
        return assetId != nullptr ? *assetId : AssetId{};
    }

    void Build( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        const Animator& animator = scene.Get<Animator>( entity );
        AnimatorRuntime runtime;
        if ( animator.mGraph )
            runtime.mGraph = mAssets.Load<AnimationGraph>( animator.mGraph );
        scene.Add<AnimatorRuntime>( entity, std::move( runtime ) );
        scene.Remove<SkinPose>( entity );
    }

    // Says what keeps an animator from playing, once until it changes.
    void Report( Entity entity, AnimatorRuntime& runtime, string message )
    {
        if ( message == runtime.mLogged )
            return;
        runtime.mLogged = message;
        LogError( "{}: animator: {}", Describe( mWorld.GetScene(), entity ), message );
    }

    // Every parameter at its default, before the first step or for a new
    // graph, and what was set meanwhile laid on.
    void StartState( AnimatorRuntime& runtime, const AnimationGraph& graph )
    {
        if ( runtime.mState.mParameters.size() == graph.mParameters.size() and
             runtime.mState.mState < static_cast<i32>( graph.mStates.size() ) )
            return;
        runtime.mState = StartAnimationGraph( graph );
        if ( runtime.mPose )
            runtime.mPose->mSeenEntered = runtime.mState.mEntered;
    }

    expected<void, string> TryApply( AnimatorRuntime& runtime,
                                     const AnimationGraph& graph,
                                     string_view name,
                                     AnimationValue value,
                                     bool trigger )
    {
        const auto parameterMaybe = graph.TryParameter( name );
        if ( not parameterMaybe )
            return unexpected( format( "no parameter {} (parameters: {})", name, Names( graph ) ) );
        const AnimationParameterKind kind = graph.mParameters[*parameterMaybe].mKind;
        if ( trigger and kind != AnimationParameterKind::Trigger )
            return unexpected( format( "{} is not a trigger: set it instead", name ) );
        if ( not trigger and kind == AnimationParameterKind::Trigger )
            return unexpected( format( "{} is a trigger: trigger it instead", name ) );
        if ( kind == AnimationParameterKind::Number and not holds_alternative<f32>( value ) )
            return unexpected( format( "{} is a number, not {}", name, DescribeValue( value ) ) );
        if ( kind == AnimationParameterKind::Bool and not holds_alternative<bool>( value ) )
            return unexpected( format( "{} is true or false, not {}", name, DescribeValue( value ) ) );
        runtime.mState.mParameters[*parameterMaybe] =
            holds_alternative<bool>( value ) ? ( get<bool>( value ) ? 1.0f : 0.0f ) : get<f32>( value );
        return {};
    }

    // Whether the animator plays: its graph, its model and the model's
    // skeleton and clips loaded, and what it plays made of them - again
    // when its model changed or any of them was reloaded.
    bool Prepare( Entity entity, AnimatorRuntime& runtime )
    {
        const AssetId modelId = ModelOf( entity );
        if ( modelId != runtime.mModelId )
        {
            runtime.mModelId = modelId;
            runtime.mModel = modelId ? mAssets.Load<ModelAsset>( modelId ) : AssetRef<ModelAsset>();
            runtime.mSkeleton = modelId ? mAssets.Load<SkeletonAsset>( modelId ) : AssetRef<SkeletonAsset>();
            runtime.mClips = modelId ? mAssets.Load<ClipsAsset>( modelId ) : AssetRef<ClipsAsset>();
            runtime.mReady = false;
        }
        if ( not runtime.mGraph )
            return Unready( entity, runtime, "it has no graph" );
        if ( not modelId )
            return Unready( entity, runtime, "its entity has no model to animate" );
        for ( const AssetEntryBase* entry : { &runtime.mGraph.Entry(), &runtime.mModel.Entry(),
                                              &runtime.mSkeleton.Entry(), &runtime.mClips.Entry() } )
        {
            if ( entry->State() == AssetState::Failed )
                return Unready( entity, runtime, entry->Error() );
            if ( entry->State() == AssetState::Loading )
                return Unready( entity, runtime, "" );
        }
        const array<u32, 4> versions{ runtime.mGraph.Entry().Version(), runtime.mModel.Entry().Version(),
                                      runtime.mSkeleton.Entry().Version(), runtime.mClips.Entry().Version() };
        if ( runtime.mReady and versions == runtime.mVersions )
            return true;

        const AnimationGraph& graph = *runtime.mGraph.TryGet();
        const ozz::animation::Skeleton& skeleton = *runtime.mSkeleton.TryGet()->mSkeleton;
        const ClipsAsset& clips = *runtime.mClips.TryGet();
        const ModelAsset& model = *runtime.mModel.TryGet();
        if ( model.mJoints.size() != static_cast<size_t>( skeleton.num_joints() ) )
            return Unready( entity, runtime,
                            format( "{} has {} joints and its skeleton {}: import it again",
                                    runtime.mModel.Entry().Path().View(), model.mJoints.size(),
                                    skeleton.num_joints() ) );
        StartState( runtime, graph );
        for ( const PendingSet& pending : runtime.mPending )
            if ( auto appliedMaybe = TryApply( runtime, graph, pending.mParameter, pending.mValue, pending.mTrigger );
                 not appliedMaybe )
                LogError( "{}: animator: {}", Describe( mWorld.GetScene(), entity ), appliedMaybe.error() );
        runtime.mPending.clear();

        runtime.mClipOf.clear();
        string missing;
        const auto find = [&]( const string& clip ) -> opt<u32> {
            if ( clip.empty() )
                return nullopt;
            const auto foundMaybe = clips.TryFind( clip );
            if ( not foundMaybe and missing.find( clip ) == string::npos )
                missing += ( missing.empty() ? "" : ", " ) + clip;
            return foundMaybe;
        };
        for ( const AnimationState& state : graph.mStates )
        {
            vector<opt<u32>>& clipOf = runtime.mClipOf.emplace_back();
            if ( state.mBlend.empty() )
                clipOf.push_back( find( state.mClip ) );
            for ( const AnimationBlendPoint& point : state.mBlend )
                clipOf.push_back( find( point.mClip ) );
        }
        // Played still: a state of a clip the model lacks holds the rest pose.
        if ( not missing.empty() )
            Report( entity, runtime,
                    format( "{} has no clip {} (clips: {})", runtime.mModel.Entry().Path().View(), missing,
                            clips.Names() ) );
        else
            runtime.mLogged.clear();

        // Ozz's buffers again when the skeleton changed.
        if ( not runtime.mPose or versions[2] != runtime.mVersions[2] or not runtime.mReady )
        {
            runtime.mPose = CreateScope<PoseBuffers>( skeleton );
            runtime.mPose->mSeenEntered = runtime.mState.mEntered;
        }
        runtime.mVersions = versions;
        runtime.mReady = true;
        return true;
    }

    bool Unready( Entity entity, AnimatorRuntime& runtime, string message )
    {
        runtime.mReady = false;
        if ( not message.empty() )
            Report( entity, runtime, std::move( message ) );
        return false;
    }

    // One fixed step of every animator's graph, in the order the animators
    // lie: what it passes heard as on_animation_event.
    void Step( f32 dt )
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Animation: step" );
        Scene& scene = mWorld.GetScene();
        vector<string> events;
        for ( const Entity entity : scene.EntitiesWith<Animator>() )
        {
            const auto runtimeMaybe = scene.TryGet<AnimatorRuntime>( entity );
            if ( not runtimeMaybe or not Prepare( entity, *runtimeMaybe ) )
                continue;
            AnimatorRuntime& runtime = *runtimeMaybe;
            const ClipsAsset& clips = *runtime.mClips.TryGet();
            const AnimationGraph& graph = *runtime.mGraph.TryGet();
            events.clear();
            StepAnimationGraph(
                graph, runtime.mState, dt, scene.Get<Animator>( entity ).mSpeed,
                [&]( string_view clip ) -> opt<f32> {
                    const auto foundMaybe = clips.TryFind( clip );
                    return foundMaybe ? opt<f32>( clips.mClips[*foundMaybe].mDuration ) : nullopt;
                },
                events );
            runtime.mRootMotion = RootMotionOf( entity, runtime, dt );
            for ( string& event : events )
                mWorld.GetEvents().Emit( ScriptCall{ entity, "on_animation_event"_n,
                                                     vector<ScriptArg>{ ScriptArg( std::move( event ) ) } } );
        }
    }

    // How fast the state the last step played moves its entity, in the world:
    // its clips' root travel, as they are blended, over the step. Nothing for
    // a state without root motion.
    AnimationRootMotion RootMotionOf( Entity entity, const AnimatorRuntime& runtime, f32 dt ) const
    {
        const AnimationGraph& graph = *runtime.mGraph.TryGet();
        const AnimationGraphState& state = runtime.mState;
        if ( state.mPlayed < 0 or dt <= 0 )
            return {};
        const auto played = static_cast<size_t>( state.mPlayed );
        const AnimationState& playing = graph.mStates[played];
        if ( not playing.mRootMotion )
            return {};
        vector<AnimationWeight> weights{ AnimationWeight{ 0, 1.0f } };
        if ( not playing.mBlend.empty() )
            weights = BlendWeights( playing.mBlend, state.mParameters[playing.mBlendParameter] );
        vec3 travel( 0.0f );
        f32 turn = 0;
        const ClipsAsset& clips = *runtime.mClips.TryGet();
        for ( const AnimationWeight& weight : weights )
        {
            const opt<u32> clipMaybe = runtime.mClipOf[played][weight.mPoint];
            if ( not clipMaybe or not clips.mClips[*clipMaybe].mRootTranslation )
                continue;
            const auto [moved, turned] =
                RootTravel( clips.mClips[*clipMaybe], state.mPlayedFrom, state.mPlayedTo, state.mPlayedWrapped );
            travel += moved * weight.mWeight;
            turn += turned * weight.mWeight;
        }
        // The model's space turned and scaled as its entity is.
        const mat3 world( ComputeWorldMatrix( mWorld.GetScene(), entity ) );
        return AnimationRootMotion{ .mVelocity = world * travel / dt, .mTurn = turn / dt };
    }

    // Each animator's pose between the last two steps, the skeletons
    // sampled side by side on Jobs' pool; then the skin poses, then what
    // hangs on joints.
    void Pose( f32 dt )
    {
        BUBBLE_PROFILE_ZONE_NAMED( "Animation: pose" );
        Scene& scene = mWorld.GetScene();
        mPosing.clear();
        for ( const Entity entity : scene.EntitiesWith<Animator>() )
        {
            const auto runtimeMaybe = scene.TryGet<AnimatorRuntime>( entity );
            if ( not runtimeMaybe )
                continue;
            if ( Prepare( entity, *runtimeMaybe ) )
                mPosing.push_back( entity );
            else
                scene.Remove<SkinPose>( entity );
        }
        // The scene only read on the pool: each animator writes its own
        // buffers.
        const Scene& reading = scene;
        const f32 alpha = mWorld.Alpha();
        mWorld.GetApp().GetJobs().ParallelFor( static_cast<u32>( mPosing.size() ), 1, [&]( u32 begin, u32 end ) {
            for ( u32 at = begin; at < end; ++at )
            {
                const AnimatorRuntime& runtime = reading.Get<AnimatorRuntime>( mPosing[at] );
                Sample( runtime, *runtime.mPose, alpha, dt );
            }
        } );
        for ( const Entity entity : mPosing )
        {
            const AnimatorRuntime& runtime = scene.Get<AnimatorRuntime>( entity );
            const ModelAsset& model = *runtime.mModel.TryGet();
            const auto poseMaybe = scene.TryGet<SkinPose>( entity );
            SkinPose& skin = poseMaybe ? *poseMaybe : scene.Add<SkinPose>( entity );
            skin.mMatrices.resize( model.mJoints.size() );
            for ( size_t j = 0; j < model.mJoints.size(); ++j )
                skin.mMatrices[j] = ToMat4( runtime.mPose->mModels[j] ) * model.mJoints[j].mInverseBind;
        }
        Attach();
    }

    // One animator's pose now: its state's clips sampled at the phase
    // between the last two steps and blended, eased out of the state before,
    // and taken into the model's space.
    static void Sample( const AnimatorRuntime& runtime, PoseBuffers& pose, f32 alpha, f32 dt )
    {
        const AnimationGraph& graph = *runtime.mGraph.TryGet();
        const ozz::animation::Skeleton& skeleton = *runtime.mSkeleton.TryGet()->mSkeleton;
        const ClipsAsset& clips = *runtime.mClips.TryGet();
        const AnimationGraphState& state = runtime.mState;

        // The clips that play, with their weights.
        array<pair<u32, f32>, 2> playing{};
        size_t count = 0;
        f32 phase = 0;
        if ( state.mState >= 0 )
        {
            const auto s = static_cast<size_t>( state.mState );
            const AnimationState& current = graph.mStates[s];
            f32 delta = state.mPhase - state.mPhaseBefore;
            if ( state.mWrapped )
                delta += delta < 0 ? 1.0f : -1.0f;
            phase = state.mPhaseBefore + delta * alpha;
            phase = current.mLoop ? phase - floor( phase ) : clamp( phase, 0.0f, 1.0f );
            if ( current.mBlend.empty() )
            {
                if ( runtime.mClipOf[s][0] )
                    playing[count++] = { *runtime.mClipOf[s][0], 1.0f };
            }
            else
                for ( const AnimationWeight& weight :
                      BlendWeights( current.mBlend, state.mParameters[current.mBlendParameter] ) )
                    if ( runtime.mClipOf[s][weight.mPoint] and weight.mWeight > 0 )
                        playing[count++] = { *runtime.mClipOf[s][weight.mPoint], weight.mWeight };
        }

        const auto rest = skeleton.joint_rest_poses();
        array<ozz::animation::BlendingJob::Layer, 2> layers{};
        array<size_t, 2> slots{};
        size_t sampled = 0;
        for ( size_t at = 0; at < count; ++at )
        {
            ozz::animation::SamplingJob sampling;
            sampling.animation = clips.mClips[playing[at].first].mAnimation.get();
            sampling.context = pose.mContexts[at].get();
            sampling.ratio = phase;
            sampling.output = Span( pose.mSampled[at] );
            if ( not sampling.Run() )
                continue;
            const SoaPose& made = pose.mSampled[at];
            layers[sampled].weight = playing[at].second;
            layers[sampled].transform = Span( made );
            slots[sampled] = at;
            ++sampled;
        }
        if ( sampled == 1 )
            pose.mLocals = pose.mSampled[slots[0]];
        else if ( sampled == 2 )
        {
            ozz::animation::BlendingJob blending;
            blending.layers = ozz::span<const ozz::animation::BlendingJob::Layer>( layers.data(), sampled );
            blending.rest_pose = rest;
            blending.output = Span( pose.mLocals );
            if ( not blending.Run() )
                copy_n( rest.begin(), rest.size(), pose.mLocals.begin() );
        }
        else
            copy_n( rest.begin(), rest.size(), pose.mLocals.begin() );

        // Eased from where the pose was when the state changed.
        SoaToJoints( pose.mLocals, pose.mJoints );
        if ( pose.mSeenEntered != state.mEntered )
        {
            pose.mSeenEntered = state.mEntered;
            if ( pose.mHistory >= 1 )
                pose.mInertializer.Begin( pose.mPrevious,
                                          pose.mHistory >= 2 ? span<const JointPose>( pose.mBeforePrevious )
                                                             : span<const JointPose>(),
                                          pose.mLastDt, pose.mJoints, state.mEnteredEasing );
        }
        const bool eased = pose.mInertializer.Active();
        pose.mInertializer.Apply( pose.mJoints, dt );
        // The pose shown is where the next easing starts; only a frame that
        // moved on measures its speed - a paused one would read as a pose
        // stopped dead.
        if ( dt > 0 and pose.mHistory >= 1 )
        {
            pose.mBeforePrevious.swap( pose.mPrevious );
            pose.mLastDt = dt;
            pose.mHistory = 2;
        }
        pose.mPrevious = pose.mJoints;
        pose.mHistory = max( pose.mHistory, 1u );
        if ( eased )
            JointsToSoa( pose.mJoints, pose.mLocals );

        const SoaPose& locals = pose.mLocals;
        ozz::animation::LocalToModelJob toModel;
        toModel.skeleton = &skeleton;
        toModel.input = Span( locals );
        toModel.output = Span( pose.mModels );
        toModel.Run();
        pose.mSampledOnce = true;
    }

    // Each bone_attachment hung on its joint of its parent's pose; one whose
    // parent does not play yet hangs from the parent itself.
    void Attach()
    {
        Scene& scene = mWorld.GetScene();
        for ( const Entity entity : scene.EntitiesWith<BoneAttachment>() )
        {
            const Entity parent = ParentOf( scene, entity );
            const auto runtimeMaybe = scene.TryGet<AnimatorRuntime>( parent );
            if ( not runtimeMaybe or not runtimeMaybe->mReady or not runtimeMaybe->mPose )
            {
                scene.Remove<ParentOffset>( entity );
                continue;
            }
            const string& bone = scene.Get<BoneAttachment>( entity ).mBone;
            const ozz::animation::Skeleton& skeleton = *runtimeMaybe->mSkeleton.TryGet()->mSkeleton;
            const auto jointMaybe = TryJointOf( skeleton, bone );
            if ( not jointMaybe )
            {
                scene.Remove<ParentOffset>( entity );
                string message = format( "{}: bone_attachment: {} has no joint {}", Describe( scene, entity ),
                                         runtimeMaybe->mModel.Entry().Path().View(), bone );
                if ( mAttachmentLogged[entity] != message )
                {
                    LogError( "{}", message );
                    mAttachmentLogged[entity] = std::move( message );
                }
                continue;
            }
            mAttachmentLogged.erase( entity );
            const mat4 joint = ToMat4( runtimeMaybe->mPose->mModels[*jointMaybe] );
            if ( const auto offsetMaybe = scene.TryGet<ParentOffset>( entity ) )
                offsetMaybe->mMatrix = joint;
            else
                scene.Add<ParentOffset>( entity, joint );
        }
    }

    // An animator's assets, while a world is restored.
    struct Held
    {
        AssetRef<AnimationGraph> mGraph;
        AssetRef<ModelAsset> mModel;
        AssetRef<SkeletonAsset> mSkeleton;
        AssetRef<ClipsAsset> mClips;
    };

    World& mWorld;
    AssetRegistry& mAssets;
    vector<AnimatorKept> mRestoring;
    vector<Held> mHeld;
    // Kept for its room: the animators posed this frame.
    vector<Entity> mPosing;
    hmap<Entity, string> mAttachmentLogged;
};

AnimationWorld& AnimationOf( World& world )
{
    return world.Get<AnimationWorld>();
}

void ScriptsOfAnimation( ScriptApi& api )
{
    api.ExtendComponent(
        Name::Declare( "animator" ),
        []( LuaTypeBuilder<ScriptComponent>& type, World& world ) {
            type.Method( "set",
                         [&world]( ScriptComponent& self, const string& parameter, LuaValue value ) {
                             AnimationWorld& animation = AnimationOf( world );
                             expected<void, string> setMaybe;
                             if ( const auto flagMaybe = value.TryAs<bool>() )
                                 setMaybe = animation.TrySet( self.mEntity, parameter, *flagMaybe );
                             else if ( const auto numberMaybe = value.TryAs<f64>() )
                                 setMaybe =
                                     animation.TrySet( self.mEntity, parameter, static_cast<f32>( *numberMaybe ) );
                             else
                                 throw LuaError( format( "animator:set( \"{}\", ... ): a number or a boolean, not {}",
                                                         parameter, value.Describe() ) );
                             if ( not setMaybe )
                                 throw LuaError( "animator:set: " + setMaybe.error() );
                         } )
                .Method( "trigger",
                         [&world]( ScriptComponent& self, const string& trigger ) {
                             if ( auto setMaybe = AnimationOf( world ).TryTrigger( self.mEntity, trigger );
                                  not setMaybe )
                                 throw LuaError( "animator:trigger: " + setMaybe.error() );
                         } )
                .Property( "state",
                           [&world]( ScriptComponent& self ) { return AnimationOf( world ).TryState( self.mEntity ); } )
                .Property( "root_velocity",
                           [&world]( ScriptComponent& self ) {
                               return AnimationOf( world ).RootMotion( self.mEntity ).mVelocity;
                           } )
                .Property( "root_turn", [&world]( ScriptComponent& self ) {
                    return AnimationOf( world ).RootMotion( self.mEntity ).mTurn;
                } );
        },
        "    function set(self, parameter: string, value: number | boolean): ()\n"
        "    function trigger(self, trigger: string): ()\n"
        "    state: string?\n"
        "    root_velocity: vector\n"
        "    root_turn: number\n" );
    api.AddCallback( "on_animation_event", "on_animation_event( self, name: string )" );
}
}

ModuleType AnimationModule( AssetRegistry& assets )
{
    return ModuleType{
        .mName = "animation",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<Animator>( "animator" )
                    .Field<&Animator::mGraph>( "graph", { .mAssetKind = cAnimationGraphKind.data() } )
                    .Field<&Animator::mSpeed>( "speed" );
                reflection.Type<BoneAttachment>( "bone_attachment" ).Field<&BoneAttachment::mBone>( "bone" );
                componentTypes.Add<Animator>();
                componentTypes.Add<BoneAttachment>();
            },
        .mCreate = [&assets]( World& world ) -> Scope<Module> { return CreateScope<Animation>( world, assets ); },
        .mScripts = &ScriptsOfAnimation,
        .mInEditor = true,
    };
}
}
