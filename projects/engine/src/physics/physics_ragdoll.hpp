#pragma once
// A skinned model's skeleton made a ragdoll's bones: which joints are bodies,
// their capsules, what each rides on; and the poses between joints, bodies
// and the skin.
#include "bubble/assets/model_asset.hpp"
#include "bubble/physics/physics.hpp"
#include "jolt.hpp"
#include "physics_joints.hpp"

namespace bubble::physics
{
// How a bone's joint bends: every way within a cone, or one way only - a
// knee back, an elbow forward.
enum class BoneBend : u8
{
    Any,
    Knee,
    Elbow,
};

// A bone a body: a capsule from its joint's origin to `mTip`, in the joint's
// own space at the model's units.
struct RagdollBone
{
    u32 mJoint = 0;
    // The bone above it, which it swings from; -1 for the first.
    i32 mParent = -1;
    vec3 mTip{ 0.0f, 1.0f, 0.0f };
    f32 mRadius = 0.05f;
    // Its share of the ragdoll's mass.
    f32 mShare = 0;
    // How far it swings off the bone above and twists about itself, radians:
    // a human's by its name, or the ragdoll's own for a name it does not know.
    f32 mSwing = 0;
    f32 mTwist = 0;
    BoneBend mBend = BoneBend::Any;
};

struct RagdollSkeleton
{
    vector<RagdollBone> mBones;
    // Each joint's bone: its own, or the one it rides on.
    vector<i32> mBoneOf;
    // A toe's joint and its foot's: the way from one to the other is what the
    // figure faces. -1 with no toe: knees and elbows bend every way.
    i32 mToe = -1;
    i32 mFoot = -1;
};

// The joint that holds `bone` to the one above, as they stand in `poses` -
// the joints', in the model's space -: its kind and limits, and its frames
// on each body, each body placed as its joint is. A knee or an elbow is a
// hinge from straight to bent all the way, however bent it is now; the rest
// swing and twist.
struct BoneJoint
{
    Joint mJoint;
    JointFrame mOwn;
    JointFrame mAbove;
};
BoneJoint MakeBoneJoint( const RagdollSkeleton& skeleton, u32 bone, const vector<mat4>& poses );

// The bones of `model` at `scale` - the entity's, from the model's units to
// metres -; none for a model with no joints.
RagdollSkeleton MakeRagdollSkeleton( const ModelAsset& model, const Ragdoll& ragdoll, f32 scale );

// Each joint's pose in the model's space, from a skin's matrices - a pose
// times its inverse bind -, or the rest when it has another length.
vector<mat4> JointPoses( const ModelAsset& model, OptRef<const SkinPose> skin );

// The skin's matrices with the bones' joints at `poses`, in the model's
// space, and the rest riding on them as at rest.
void PoseSkin( const ModelAsset& model, const RagdollSkeleton& skeleton, vector<mat4>& poses, SkinPose& skin );

// Where an entity stands once its ragdoll lets go, in the world: turned
// about the world's up as far as its first bone - the pelvis - turned from
// where it is at rest - no lean, whichever way the body lies -, on `ground`
// under the pelvis, or, with none, as far below the pelvis as it is at rest.
// `placed` is the entity as it stood when it fell, `rest` the first bone at
// rest in the entity's space - in metres -, `bone` where it lies now.
Pose StandingPose( const Pose& placed, const Pose& rest, const Pose& bone, opt<vec3> ground );
}
