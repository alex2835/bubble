#pragma once
#include "engine/types/number.hpp"
#include "engine/types/array.hpp"
#include "engine/types/pointer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/animation/inertialization.hpp"
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

    // One clip's contribution to the pose: where in it, 0..1, and how much.
    struct Layer
    {
        const AnimationClip* mClip = nullptr;
        f32 mRatio = 0.0f;
        f32 mWeight = 1.0f;
    };

    // Blends the layers by weight (normalised by ozz, so they need not sum to
    // one), carries any transition in flight, and poses the skeleton. No
    // layers - or none with a clip and a weight - poses it at rest, which is
    // what a model with no clips draws as. `dt` is the frame's step: the
    // transition advances by it, and it is what the next transition measures
    // the pose's velocity over.
    void Sample( std::span<const Layer> layers, f32 dt );
    // Samples the clip at `time` seconds.
    void Sample( const AnimationClip* clip, f32 time, f32 dt );
    // The next Sample eases the pose from where it is now into whatever it
    // samples, over `seconds`, without the outgoing clip - see Inertializer.
    // Called at the switch, before the Sample that plays the new clip.
    void BeginTransition( f32 seconds );
    bool InTransition() const { return mInertializer.Active(); }
    f32 TransitionProgress() const { return mInertializer.Progress(); }
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

    void LocalToModel( ozz::span<const ozz::math::SoaTransform> locals );

    Ref<Model> mModel;
    // One per layer. A context caches where it last sampled in an animation
    // and starts over on a different one, so layer i keeps context i and a
    // steady blend samples every clip warm.
    vector<Scope<ozz::animation::SamplingJob::Context>> mContexts;
    vector<ozz::vector<ozz::math::SoaTransform>> mLayerLocals;
    ozz::vector<ozz::math::SoaTransform> mLocals;

    // The pose as shown, per joint, this frame and the two before it - what a
    // transition starts from. Kept after the inertializer has had its say, so
    // a transition out of a transition continues the motion it interrupts.
    vector<JointPose> mPose;
    vector<JointPose> mPreviousPose;
    vector<JointPose> mBeforePreviousPose;
    u32 mHistory = 0;
    f32 mLastDt = 0.0f;
    Inertializer mInertializer;
    f32 mPendingTransition = 0.0f;
    ozz::vector<ozz::math::Float4x4> mModels;
    // mModels * inverse bind, what the skinning reads.
    ozz::vector<ozz::math::Float4x4> mSkinMatrices;
    vector<SkinnedMesh> mSkinnedMeshes;
    // Index into mSkinnedMeshes per model mesh, -1 for an unskinned one.
    vector<i32> mSkinnedMeshOfMesh;
};

}
