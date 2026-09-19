#include "engine/pch/pch.hpp"
#include "engine/animation/animator.hpp"
#include <ozz/animation/runtime/skeleton.h>
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

    for ( auto& context : mContexts )
        context.Resize( skeleton.num_joints() );
    for ( auto& locals : mLayerLocals )
        locals.resize( skeleton.num_soa_joints() );
    mLocals.resize( skeleton.num_soa_joints() );
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


bool Animator::SampleLayer( const Layer& layer, size_t index )
{
    const AnimationClip* clip = layer.mClip;
    if ( not clip or not clip->mAnimation or layer.mWeight <= 0.0f )
        return false;

    ozz::animation::SamplingJob sampling;
    sampling.animation = clip->mAnimation.get();
    sampling.context = &mContexts[index];
    sampling.ratio = clip->mDuration > 0.0f ? std::clamp( layer.mTime / clip->mDuration, 0.0f, 1.0f ) : 0.0f;
    sampling.output = ozz::make_span( mLayerLocals[index] );
    return sampling.Run();
}


void Animator::Sample( const AnimationClip* clip, f32 time )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;
    if ( SampleLayer( { clip, time, 1.0f }, 0 ) )
        LocalToModel( ozz::make_span( mLayerLocals[0] ) );
    else
        LocalToModel( skeleton.joint_rest_poses() );
}


void Animator::Sample( const Layer& from, const Layer& to )
{
    const ozz::animation::Skeleton& skeleton = *mModel->mSkeleton->mSkeleton;

    ozz::animation::BlendingJob::Layer layers[2];
    size_t count = 0;
    const Layer* inputs[2] = { &from, &to };
    for ( size_t i = 0; i < 2; i++ )
    {
        if ( not SampleLayer( *inputs[i], i ) )
            continue;
        layers[count].weight = inputs[i]->mWeight;
        layers[count].transform = ozz::make_span( mLayerLocals[i] );
        count++;
    }

    if ( count == 0 )
    {
        LocalToModel( skeleton.joint_rest_poses() );
        return;
    }

    // Weights are normalised by the job, so a fade is (1 - t) against t and
    // the pose never dips towards rest halfway through.
    ozz::animation::BlendingJob blending;
    blending.layers = { layers, count };
    blending.rest_pose = skeleton.joint_rest_poses();
    blending.output = ozz::make_span( mLocals );
    if ( blending.Run() )
        LocalToModel( ozz::make_span( mLocals ) );
    else
        LocalToModel( layers[0].transform );
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
