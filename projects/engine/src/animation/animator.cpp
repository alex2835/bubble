#include "engine/pch/pch.hpp"
#include "engine/animation/animator.hpp"
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
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


Animator::Animator( Ref<Model> model )
    : mModel( std::move( model ) )
{
    BUBBLE_ASSERT( mModel and mModel->Skinned(), "Animator needs a skinned model" );
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    mContext.Resize( skeleton.num_joints() );
    mLocals.resize( skeleton.num_soa_joints() );
    mPose.resize( skeleton.num_joints() );
    mPreviousPose.resize( skeleton.num_joints() );
    mBeforePreviousPose.resize( skeleton.num_joints() );
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


void Animator::BeginTransition( f32 seconds )
{
    mPendingTransition = std::max( seconds, 0.0f );
}


void Animator::Sample( const AnimationClip* clip, f32 time, f32 dt )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    bool sampled = false;
    if ( clip and clip->mAnimation )
    {
        ozz::animation::SamplingJob sampling;
        sampling.animation = clip->mAnimation.get();
        sampling.context = &mContext;
        sampling.ratio = clip->mDuration > 0.0f ? std::clamp( time / clip->mDuration, 0.0f, 1.0f ) : 0.0f;
        sampling.output = ozz::make_span( mLocals );
        sampled = sampling.Run();
    }
    SoaToJoints( sampled ? ozz::make_span( mLocals ) : skeleton.joint_rest_poses(), mPose );

    // A transition needs the frame before it to start from; on the first
    // frame there is none, and the new clip simply shows.
    if ( mPendingTransition > 0.0f )
    {
        if ( mHistory >= 1 )
            mInertializer.Begin( mPreviousPose,
                                 mHistory >= 2 ? std::span<const JointPose>( mBeforePreviousPose )
                                               : std::span<const JointPose>(),
                                 mLastDt, mPose, mPendingTransition );
        mPendingTransition = 0.0f;
    }
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

    if ( mInertializer.Active() or not sampled )
        JointsToSoa( mPose, ozz::make_span( mLocals ) );
    LocalToModel( ozz::make_span( mLocals ) );
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
