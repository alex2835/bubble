#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
class AssetRegistry;

// Plays an animation graph (bubble/animation/animation_graph.hpp) on its
// entity's model: the skeleton and clips imported beside the model, from
// the same source, posed each frame, and the model drawn so. Its
// parameters start at the graph's defaults; scripts set them.
struct Animator
{
    AssetId mGraph;
    // How fast it all plays: 1 as made, 0 standing.
    f32 mSpeed = 1;
};

// Hangs its entity on a joint of its parent's animated model - a sword in a
// hand: its transform is then from that joint, not from the parent.
struct BoneAttachment
{
    string mBone;
};

// How fast the state an animator plays moves its entity, as its clips' root
// went in the last step: in the world, metres a second, and turning about
// the up axis, radians a second. Nothing for a state without root motion.
struct AnimationRootMotion
{
    vec3 mVelocity{ 0.0f };
    f32 mTurn = 0;
};

// A world's animators as scripts and tools reach them: a service. An
// entity's animator plays once its graph, its model and the model's skeleton
// and clips are loaded; what is set before then waits for it.
class AnimationWorld
{
public:
    virtual ~AnimationWorld() = default;

    // A number parameter, or a bool one. The error says what is wrong: no
    // animator, no such parameter (and the ones there are), a value of the
    // other kind, a trigger - which TryTrigger sets.
    virtual expected<void, string> TrySet( Entity entity, string_view parameter, f32 value ) = 0;
    virtual expected<void, string> TrySet( Entity entity, string_view parameter, bool value ) = 0;
    virtual expected<void, string> TryTrigger( Entity entity, string_view trigger ) = 0;
    // The state it plays; nothing before its first step.
    virtual opt<string> TryState( Entity entity ) const = 0;
    // Nothing for an entity with no animator.
    virtual AnimationRootMotion RootMotion( Entity entity ) const = 0;
    // Where a joint is this frame, in the space of the model; nothing before
    // the animator plays, or for a joint its skeleton lacks.
    virtual opt<mat4> TryJoint( Entity entity, string_view joint ) const = 0;
};

// "animator" and "bone_attachment"; AnimationWorld. The graph's logic -
// clips' time, transitions, events - goes on each fixed step, in
// Phase::Physics; poses are sampled each frame between the last two steps,
// in Phase::Animation, the models' skin poses (SkinPose) and attached
// entities' places (ParentOffset) written for what comes after.
ModuleType AnimationModule( AssetRegistry& assets );
}
