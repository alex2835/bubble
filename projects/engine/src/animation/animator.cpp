#include "engine/pch/pch.hpp"
#include "engine/animation/animator.hpp"
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/geometry/runtime/skinning_job.h>
#include <ozz/base/span.h>

namespace bubble
{
namespace
{
// Float4x4 is four SimdFloat4 columns; mat4 is four vec4 columns. Same bytes,
// in both the SSE and the reference build.
static_assert( sizeof( ozz::math::Float4x4 ) == sizeof( mat4 ) );

ozz::math::Float4x4 ToOzz( const mat4& m )
{
    ozz::math::Float4x4 result;
    memcpy( &result, &m, sizeof( result ) );
    return result;
}

// ozz keeps four joints per SoaTransform, one component across the four
// lanes; the inertializer wants one struct per joint.
void SoaToJoints( ozz::span<const ozz::math::SoaTransform> soa, vector<JointPose>& joints )
{
    for ( size_t j = 0; j < joints.size(); j++ )
    {
        const ozz::math::SoaTransform& t = soa[j / 4];
        const size_t lane = j % 4;
        float tx[4], ty[4], tz[4], rx[4], ry[4], rz[4], rw[4], sx[4], sy[4], sz[4];
        ozz::math::StorePtrU( t.translation.x, tx );
        ozz::math::StorePtrU( t.translation.y, ty );
        ozz::math::StorePtrU( t.translation.z, tz );
        ozz::math::StorePtrU( t.rotation.x, rx );
        ozz::math::StorePtrU( t.rotation.y, ry );
        ozz::math::StorePtrU( t.rotation.z, rz );
        ozz::math::StorePtrU( t.rotation.w, rw );
        ozz::math::StorePtrU( t.scale.x, sx );
        ozz::math::StorePtrU( t.scale.y, sy );
        ozz::math::StorePtrU( t.scale.z, sz );
        joints[j].mTranslation = vec3( tx[lane], ty[lane], tz[lane] );
        joints[j].mRotation = glm::quat( rw[lane], rx[lane], ry[lane], rz[lane] );
        joints[j].mScale = vec3( sx[lane], sy[lane], sz[lane] );
    }
}

void JointsToSoa( const vector<JointPose>& joints, ozz::span<ozz::math::SoaTransform> soa )
{
    for ( size_t s = 0; s < soa.size(); s++ )
    {
        // Lanes past the last joint keep the identity, as ozz pads them.
        JointPose lanes[4];
        for ( size_t lane = 0; lane < 4; lane++ )
            if ( s * 4 + lane < joints.size() )
                lanes[lane] = joints[s * 4 + lane];
        const auto load = [&]( auto member, auto component )
        {
            return ozz::math::simd_float4::Load( ( lanes[0].*member ).*component, ( lanes[1].*member ).*component,
                                                 ( lanes[2].*member ).*component, ( lanes[3].*member ).*component );
        };
        soa[s].translation = { load( &JointPose::mTranslation, &vec3::x ),
                               load( &JointPose::mTranslation, &vec3::y ),
                               load( &JointPose::mTranslation, &vec3::z ) };
        soa[s].rotation = { load( &JointPose::mRotation, &glm::quat::x ),
                            load( &JointPose::mRotation, &glm::quat::y ),
                            load( &JointPose::mRotation, &glm::quat::z ),
                            load( &JointPose::mRotation, &glm::quat::w ) };
        soa[s].scale = { load( &JointPose::mScale, &vec3::x ),
                         load( &JointPose::mScale, &vec3::y ),
                         load( &JointPose::mScale, &vec3::z ) };
    }
}

// The job takes strides in bytes and reads its inputs as floats.
template <typename T>
ozz::span<const float> FloatSpan( const vector<T>& v )
{
    return { reinterpret_cast<const float*>( v.data() ), v.size() * sizeof( T ) / sizeof( float ) };
}
template <typename T>
ozz::span<float> FloatSpan( vector<T>& v )
{
    return { reinterpret_cast<float*>( v.data() ), v.size() * sizeof( T ) / sizeof( float ) };
}
}


// PoseTrack

PoseTrack::PoseTrack( u32 jointCount )
    : mPose( jointCount ),
      mPreviousPose( jointCount ),
      mBeforePreviousPose( jointCount )
{
}

void PoseTrack::BeginTransition( f32 seconds )
{
    mPendingTransition = std::max( seconds, 0.0f );
}

void PoseTrack::Apply( Pose& pose, f32 dt )
{
    SoaToJoints( ozz::make_span( pose ), mPose );

    // A transition needs the frame before it to start from; on the first
    // frame there is none, and the new pose simply shows.
    if ( mPendingTransition > 0.0f )
    {
        if ( mHistory >= 1 )
            mInertializer.Begin( mPreviousPose,
                                 mHistory >= 2 ? std::span<const JointPose>( mBeforePreviousPose )
                                               : std::span<const JointPose>(),
                                 mLastDt, mPose, mPendingTransition );
        mPendingTransition = 0.0f;
    }
    const bool eased = mInertializer.Active();
    mInertializer.Apply( mPose, dt );

    // Only a frame that actually advanced is history: a paused editor frame
    // would otherwise read as a pose that stopped dead, and the transition
    // out of it would start from zero velocity.
    if ( dt > 0.0f or mHistory == 0 )
    {
        mBeforePreviousPose.swap( mPreviousPose );
        mPreviousPose = mPose;
        mLastDt = dt;
        mHistory = std::min( mHistory + 1, 2u );
    }

    if ( eased )
        JointsToSoa( mPose, ozz::make_span( pose ) );
}


// Animator

Animator::Animator( Ref<Model> model )
    : mModel( std::move( model ) )
{
    BUBBLE_ASSERT( mModel and mModel->Skinned(), "Animator needs a skinned model" );
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    mComposed.resize( skeleton.num_soa_joints() );
    mBaseMask.resize( skeleton.num_soa_joints() );
    mModels.resize( skeleton.num_joints() );
    mSkinMatrices.resize( skeleton.num_joints() );

    mSkinnedMeshOfMesh.assign( mModel->mMeshes.size(), -1 );
    for ( size_t i = 0; i < mModel->mMeshes.size(); i++ )
    {
        const Mesh& mesh = mModel->mMeshes[i];
        if ( mesh.mSkin.Empty() )
            continue;
        mSkinnedMeshOfMesh[i] = static_cast<i32>( mSkinnedMeshes.size() );
        SkinnedMesh& skinned = mSkinnedMeshes.emplace_back();
        skinned.mMeshIndex = i;
        // Texture coordinates never change; the rest is overwritten each
        // frame. Copying everything keeps the layout identical to the mesh's
        // own, so the same pipeline draws either.
        skinned.mVertices = mesh.mVertices;
        skinned.mBuffers.SetBufferData( skinned.mVertices, mesh.mIndices );
    }
}

u32 Animator::JointCount() const
{
    return static_cast<u32>( mModel->mSkeleton->mSkeleton->num_joints() );
}

Pose Animator::MakePose() const
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;
    const auto rest = skeleton.joint_rest_poses();
    return Pose( rest.begin(), rest.end() );
}

PoseTrack& Animator::Track( u32 slot )
{
    while ( mTracks.size() <= slot )
        mTracks.push_back( CreateScope<PoseTrack>( JointCount() ) );
    return *mTracks[slot];
}


void Animator::SamplePose( u32 slot, std::span<const Layer> layers, Pose& out )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;
    if ( mContexts.size() <= slot )
        mContexts.resize( slot + 1 );
    auto& contexts = mContexts[slot];

    // Sample every layer that counts, each into its own buffer.
    vector<ozz::animation::BlendingJob::Layer> blendLayers;
    for ( size_t i = 0; i < layers.size(); i++ )
    {
        const Layer& layer = layers[i];
        if ( not layer.mClip or not layer.mClip->mAnimation or layer.mWeight <= 0.0f )
            continue;

        while ( contexts.size() <= i )
            contexts.push_back( CreateScope<ozz::animation::SamplingJob::Context>( skeleton.num_joints() ) );
        while ( mLayerLocals.size() <= i )
            mLayerLocals.emplace_back().resize( skeleton.num_soa_joints() );

        const ozz::animation::Animation* animation = layer.mAdditive     ? layer.mClip->Additive()
                                                    : layer.mRootMotion ? layer.mRootMotion->mAnimation.get()
                                                                        : layer.mClip->mAnimation.get();
        if ( not animation )
            continue;
        ozz::animation::SamplingJob sampling;
        sampling.animation = animation;
        sampling.context = contexts[i].get();
        sampling.ratio = std::clamp( layer.mRatio, 0.0f, 1.0f );
        sampling.output = ozz::make_span( mLayerLocals[i] );
        if ( not sampling.Run() )
            continue;

        ozz::animation::BlendingJob::Layer& blendLayer = blendLayers.emplace_back();
        blendLayer.weight = layer.mWeight;
        blendLayer.transform = ozz::make_span( mLayerLocals[i] );
    }

    if ( blendLayers.size() == 1 )
    {
        // One layer is that layer; no reason to run the blend.
        std::ranges::copy( blendLayers[0].transform, out.begin() );
        return;
    }
    if ( not blendLayers.empty() )
    {
        ozz::animation::BlendingJob blending;
        blending.layers = ozz::make_span( blendLayers );
        blending.rest_pose = skeleton.joint_rest_poses();
        blending.output = ozz::make_span( out );
        if ( blending.Run() )
            return;
    }
    std::ranges::copy( skeleton.joint_rest_poses(), out.begin() );
}


const JointMask& Animator::Mask( std::span<const string> joints )
{
    string key;
    for ( const string& joint : joints )
        key += joint + '\n';
    auto iter = mMasks.find( key );
    if ( iter != mMasks.end() )
        return iter->second;

    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;
    vector<f32> weights( skeleton.num_joints(), 0.0f );
    for ( const string& entry : joints )
    {
        const bool exclude = entry.starts_with( '!' );
        const string_view name = exclude ? string_view( entry ).substr( 1 ) : string_view( entry );
        const auto root = mModel->mSkeleton->JointIndex( name );
        if ( not root )
        {
            LogWarning( "Animator: mask joint '{}' is not in the skeleton of '{}'", name, mModel->mName );
            continue;
        }
        // ozz orders joints depth first, parents before children, so a
        // subtree is the run of joints from the root while the parent is
        // at or below it - which is what IterateJointsDF walks.
        ozz::animation::IterateJointsDF( skeleton, [&]( int joint, int ) { weights[joint] = exclude ? 0.0f : 1.0f; }, *root );
    }

    JointMask mask( skeleton.num_soa_joints() );
    for ( int s = 0; s < skeleton.num_soa_joints(); s++ )
    {
        const auto lane = [&]( int l ) { const int j = s * 4 + l; return j < skeleton.num_joints() ? weights[j] : 0.0f; };
        mask[s] = ozz::math::simd_float4::Load( lane( 0 ), lane( 1 ), lane( 2 ), lane( 3 ) );
    }
    return mMasks.emplace( std::move( key ), std::move( mask ) ).first->second;
}


void Animator::Compose( const Pose& base, std::span<const Overlay> overlays )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    vector<ozz::animation::BlendingJob::Layer> layers;
    vector<ozz::animation::BlendingJob::Layer> additiveLayers;
    for ( const Overlay& overlay : overlays )
    {
        if ( not overlay.mPose or overlay.mWeight <= 0.0f )
            continue;
        ozz::animation::BlendingJob::Layer& layer = ( overlay.mAdditive ? additiveLayers : layers ).emplace_back();
        layer.weight = overlay.mWeight;
        layer.transform = ozz::make_span( *overlay.mPose );
        if ( overlay.mMask )
            layer.joint_weights = ozz::make_span( *overlay.mMask );
    }
    if ( layers.empty() and additiveLayers.empty() )
    {
        LocalToModel( ozz::make_span( base ) );
        return;
    }

    // An overlay at weight w within its mask should show w of itself and
    // 1 - w of the base, not (base + w overlay) / (1 + w): the base's own
    // per joint weight is what the overlays leave, so the job's
    // normalisation lands on the lerp.
    const ozz::math::SimdFloat4 one = ozz::math::simd_float4::one();
    const ozz::math::SimdFloat4 zero = ozz::math::simd_float4::zero();
    for ( int s = 0; s < skeleton.num_soa_joints(); s++ )
    {
        ozz::math::SimdFloat4 taken = zero;
        for ( const Overlay& overlay : overlays )
        {
            if ( not overlay.mPose or overlay.mWeight <= 0.0f or overlay.mAdditive )
                continue;
            const ozz::math::SimdFloat4 w = overlay.mMask ? ( *overlay.mMask )[s] * ozz::math::simd_float4::Load1( overlay.mWeight )
                                                          : ozz::math::simd_float4::Load1( overlay.mWeight );
            taken = ozz::math::Min( one, taken + w );
        }
        mBaseMask[s] = one - taken;
    }
    ozz::animation::BlendingJob::Layer& baseLayer = layers.emplace_back();
    baseLayer.weight = 1.0f;
    baseLayer.transform = ozz::make_span( base );
    baseLayer.joint_weights = ozz::make_span( mBaseMask );

    ozz::animation::BlendingJob blending;
    blending.layers = ozz::make_span( layers );
    blending.additive_layers = ozz::make_span( additiveLayers );
    blending.rest_pose = skeleton.joint_rest_poses();
    blending.output = ozz::make_span( mComposed );
    if ( blending.Run() )
        LocalToModel( ozz::make_span( mComposed ) );
    else
        LocalToModel( ozz::make_span( base ) );
}


void Animator::LocalToModel( ozz::span<const ozz::math::SoaTransform> locals )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    ozz::animation::LocalToModelJob localToModel;
    localToModel.skeleton = &skeleton;
    localToModel.input = locals;
    localToModel.output = ozz::make_span( mModels );
    localToModel.Run();

    const vector<mat4>& inverseBind = mModel->mSkeleton->mInverseBind;
    for ( size_t j = 0; j < mModels.size(); j++ )
        mSkinMatrices[j] = mModels[j] * ToOzz( inverseBind[j] );
}


void Animator::Skin()
{
    for ( SkinnedMesh& skinned : mSkinnedMeshes )
    {
        const Mesh& mesh = mModel->mMeshes[skinned.mMeshIndex];
        const MeshSkin& skin = mesh.mSkin;
        const VertexBufferData& in = mesh.mVertices;
        VertexBufferData& out = skinned.mVertices;

        ozz::geometry::SkinningJob job;
        job.vertex_count = static_cast<int>( in.VertexCount() );
        job.influences_count = 4;
        job.joint_matrices = ozz::make_span( mSkinMatrices );
        job.joint_indices = { &skin.mJointIndices[0].x, skin.mJointIndices.size() * 4 };
        job.joint_indices_stride = sizeof( glm::u16vec4 );
        // The job reads influences_count - 1 weights and restores the last
        // from the sum, which is why the weights had to be normalised on
        // import.
        job.joint_weights = FloatSpan( skin.mJointWeights );
        job.joint_weights_stride = sizeof( vec4 );
        job.in_positions = FloatSpan( in.mPositions );
        job.in_positions_stride = sizeof( vec3 );
        job.out_positions = FloatSpan( out.mPositions );
        job.out_positions_stride = sizeof( vec3 );
        job.in_normals = FloatSpan( in.mNormals );
        job.in_normals_stride = sizeof( vec3 );
        job.out_normals = FloatSpan( out.mNormals );
        job.out_normals_stride = sizeof( vec3 );
        job.in_tangents = FloatSpan( in.mTangents );
        job.in_tangents_stride = sizeof( vec3 );
        job.out_tangents = FloatSpan( out.mTangents );
        job.out_tangents_stride = sizeof( vec3 );
        if ( not job.Run() )
        {
            LogError( "Animator: skinning failed for mesh '{}' of '{}'", mesh.mName, mModel->mName );
            continue;
        }

        // The job leaves bitangents to the caller: a cross product is cheaper
        // than skinning a third vector. The handedness is the mesh's own.
        for ( size_t v = 0; v < out.mBitangents.size(); v++ )
        {
            const f32 sign = glm::dot( glm::cross( in.mNormals[v], in.mTangents[v] ), in.mBitangents[v] ) < 0.0f
                             ? -1.0f : 1.0f;
            out.mBitangents[v] = glm::cross( out.mNormals[v], out.mTangents[v] ) * sign;
        }

        skinned.mBuffers.SetBufferData( out, mesh.mIndices );
    }
}


const MeshBuffers* Animator::SkinnedBuffers( size_t meshIndex ) const
{
    if ( meshIndex >= mSkinnedMeshOfMesh.size() or mSkinnedMeshOfMesh[meshIndex] < 0 )
        return nullptr;
    return &mSkinnedMeshes[mSkinnedMeshOfMesh[meshIndex]].mBuffers;
}


std::span<const mat4> Animator::JointMatrices() const
{
    return { reinterpret_cast<const mat4*>( mModels.data() ), mModels.size() };
}

}
