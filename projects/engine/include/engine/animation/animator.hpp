#pragma once
#include "engine/types/number.hpp"
#include "engine/types/array.hpp"
#include "engine/types/pointer.hpp"
#include "engine/renderer/model.hpp"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
#include <span>

namespace bubble
{
// One entity's animation state for one skinned model: the sampled pose and
// the posed vertices. Created by the engine's animation update for an entity
// that has an AnimatorComponent and a skinned ModelComponent, and replaced
// when the entity's model changes.
//
// Skinning runs on the CPU. Each frame the pose is sampled, taken to model
// space, multiplied by the inverse bind matrices, and ozz's SkinningJob
// writes posed positions, normals and tangents for every skinned mesh into a
// vertex buffer this object owns. The renderer draws the model's meshes as
// usual but binds these buffers in place of the mesh's own - the material,
// indices and everything else stay the model's, so nothing is duplicated per
// instance but the vertices that actually change.
class Animator
{
public:
    explicit Animator( Ref<Model> model );
    Animator( const Animator& ) = delete;
    Animator& operator=( const Animator& ) = delete;

    const Ref<Model>& GetModel() const { return mModel; }

    // A clip and where in it, with a weight to blend against the other layer.
    struct Layer
    {
        const AnimationClip* mClip = nullptr;
        f32 mTime = 0.0f;
        f32 mWeight = 1.0f;
    };

    // Samples the clip at `time` seconds and poses the skeleton; a null clip
    // poses it at rest, which is what a model with no clips draws as.
    void Sample( const AnimationClip* clip, f32 time );
    // Blends two clips by their weights - a cross fade has `from` fading out
    // as `to` fades in. A layer with a null clip or no weight is left out.
    void Sample( const Layer& from, const Layer& to );
    // Writes the posed vertices for the current pose. Must follow Sample.
    void Skin();

    // The posed vertex buffers for the model's mesh at `meshIndex`, or null
    // for a mesh with no skin, which draws from its own buffers.
    const MeshBuffers* SkinnedBuffers( size_t meshIndex ) const;

    // Model space joint matrices of the current pose, in skeleton order.
    std::span<const mat4> JointMatrices() const;

private:
    struct SkinnedMesh
    {
        size_t mMeshIndex = 0;
        VertexBufferData mVertices;
        MeshBuffers mBuffers;
    };

    // Samples `layer` into mLayerLocals[index]; false when there is nothing
    // to sample.
    bool SampleLayer( const Layer& layer, size_t index );
    void LocalToModel( ozz::span<const ozz::math::SoaTransform> locals );

    Ref<Model> mModel;
    // One per blend layer: a context caches where it last sampled in one
    // animation, and would start over every frame if two clips shared it.
    ozz::animation::SamplingJob::Context mContexts[2];
    ozz::vector<ozz::math::SoaTransform> mLayerLocals[2];
    ozz::vector<ozz::math::SoaTransform> mLocals;
    ozz::vector<ozz::math::Float4x4> mModels;
    // mModels * inverse bind, what the skinning reads.
    ozz::vector<ozz::math::Float4x4> mSkinMatrices;
    vector<SkinnedMesh> mSkinnedMeshes;
    // Index into mSkinnedMeshes per model mesh, -1 for an unskinned one.
    vector<i32> mSkinnedMeshOfMesh;
};

}
