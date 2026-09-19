#pragma once
#include "engine/types/number.hpp"
#include "engine/types/array.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/renderer/model.hpp"
#include "engine/animation/inertialization.hpp"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
#include <span>

namespace bubble
{
// A local space pose, four joints per element, as ozz samples and blends it.
using Pose = ozz::vector<ozz::math::SoaTransform>;

// A per joint weight, four joints per element: which part of the skeleton an
// overlay layer applies to.
using JointMask = ozz::vector<ozz::math::SimdFloat4>;


// One stream of poses with its own transition easing - the base playback, or
// an overlay layer. Keeps the last two poses it showed, which is what an
// inertialized transition starts from (see Inertializer), so a stream that
// switches clips eases on its own, independent of what the other streams do.
class PoseTrack
{
public:
    explicit PoseTrack( u32 jointCount );

    // The next Apply eases the pose from where it is now into whatever it is
    // handed, over `seconds`. Called at the switch, before that frame's
    // Apply.
    void BeginTransition( f32 seconds );
    // Carries `pose` - this frame's sample - through any transition in
    // flight, and records it as history. `dt` advances the transition and
    // is what the next one measures the pose's velocity over.
    void Apply( Pose& pose, f32 dt );

    bool InTransition() const { return mInertializer.Active(); }
    f32 TransitionProgress() const { return mInertializer.Progress(); }

private:
    vector<JointPose> mPose;
    vector<JointPose> mPreviousPose;
    vector<JointPose> mBeforePreviousPose;
    u32 mHistory = 0;
    f32 mLastDt = 0.0f;
    Inertializer mInertializer;
    f32 mPendingTransition = 0.0f;
};


// One entity's animation for one skinned model: the poses, and the posed
// vertices. Created by the AnimatorComponent for an entity that has a skinned
// ModelComponent, and replaced when the entity's model changes.
//
// A frame is: SamplePose for the base and for each overlay, each stream's
// PoseTrack easing its own, then Compose to lay the overlays over the base by
// their masks and take the result to model space, then Skin.
//
// Skinning runs on the CPU. ozz's SkinningJob writes posed positions, normals
// and tangents for every skinned mesh into a vertex buffer this object owns.
// The renderer draws the model's meshes as usual but binds these buffers in
// place of the mesh's own - the material, indices and everything else stay
// the model's, so nothing is duplicated per instance but the vertices that
// actually change.
class Animator
{
public:
    explicit Animator( Ref<Model> model );
    Animator( const Animator& ) = delete;
    Animator& operator=( const Animator& ) = delete;

    const Ref<Model>& GetModel() const { return mModel; }
    u32 JointCount() const;

    // One clip's contribution to a pose: where in it, 0..1, and how much.
    struct Layer
    {
        const AnimationClip* mClip = nullptr;
        f32 mRatio = 0.0f;
        f32 mWeight = 1.0f;
    };

    // A pose sized for this skeleton, at rest.
    Pose MakePose() const;
    // Blends the layers by weight (normalised by ozz, so they need not sum
    // to one) into `out`. No layers - or none with a clip and a weight -
    // gives the rest pose. Sampling caches where each slot last was in each
    // layer's clip; a stream keeps its slot so its clips sample warm.
    void SamplePose( u32 slot, std::span<const Layer> layers, Pose& out );
    // The track for a stream: 0 is the base, overlays follow.
    PoseTrack& Track( u32 slot );

    // A mask from joint names, each taken with its subtree, a name with a
    // leading "!" taking its subtree back out. Cached per set of names. A
    // name the skeleton lacks is skipped, with a warning once.
    const JointMask& Mask( std::span<const string> joints );

    // An overlay to lay over the base.
    struct Overlay
    {
        const Pose* mPose = nullptr;
        const JointMask* mMask = nullptr;
        f32 mWeight = 1.0f;
    };

    // Lays the overlays over `base` - each replacing the base by its weight
    // within its mask - and poses the skeleton with the result. Must precede
    // Skin.
    void Compose( const Pose& base, std::span<const Overlay> overlays );
    // Writes the posed vertices for the current pose.
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
    // [slot][layer]. A context caches where it last sampled in an animation
    // and starts over on a different one.
    vector<vector<Scope<ozz::animation::SamplingJob::Context>>> mContexts;
    vector<Pose> mLayerLocals;
    vector<Scope<PoseTrack>> mTracks;
    hash_map<string, JointMask> mMasks;
    Pose mComposed;
    // The base's per joint weights for a compose, 1 - what the overlays take.
    JointMask mBaseMask;
    ozz::vector<ozz::math::Float4x4> mModels;
    // mModels * inverse bind, what the skinning reads.
    ozz::vector<ozz::math::Float4x4> mSkinMatrices;
    vector<SkinnedMesh> mSkinnedMeshes;
    // Index into mSkinnedMeshes per model mesh, -1 for an unskinned one.
    vector<i32> mSkinnedMeshOfMesh;
};

}
