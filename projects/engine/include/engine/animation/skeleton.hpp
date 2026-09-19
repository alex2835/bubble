 #pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"
#include "engine/types/glm.hpp"
#include <ozz/base/memory/unique_ptr.h>
#include <optional>

#include <glm/gtc/quaternion.hpp>

namespace ozz::animation { class Skeleton; class Animation; class Float3Track; class QuaternionTrack; }
namespace ozz::animation::offline { struct RawAnimation; }

namespace bubble
{
// The joint hierarchy a skinned model is bound to, plus the one thing ozz's
// skeleton does not carry: the inverse bind matrix per joint, which takes a
// vertex from mesh space into the joint's space so a posed joint can bring it
// back out. Both are in ozz joint order - parents before children - and every
// index handed to a SamplingJob, LocalToModelJob or SkinningJob is that order.
//
// Owned by the Model it came in with, shared by every entity drawing it.
struct Skeleton
{
    Skeleton();
    ~Skeleton();
    Skeleton( Skeleton&& );
    Skeleton& operator=( Skeleton&& );
    Skeleton( const Skeleton& ) = delete;
    Skeleton& operator=( const Skeleton& ) = delete;

    u32 JointCount() const;
    std::optional<u16> JointIndex( string_view name ) const;

public:
    ozz::unique_ptr<ozz::animation::Skeleton> mSkeleton;
    vector<mat4> mInverseBind;
    str_hash_map<u16> mJointByName;
};


// One clip, compressed by ozz for the skeleton it was imported alongside. A
// clip only samples correctly against that skeleton: its tracks are in the
// skeleton's joint order.
struct AnimationClip
{
    AnimationClip();
    ~AnimationClip();
    AnimationClip( AnimationClip&& );
    AnimationClip& operator=( AnimationClip&& );
    AnimationClip( const AnimationClip& ) = delete;
    AnimationClip& operator=( const AnimationClip& ) = delete;

    // The clip as a delta from its own first frame, for an additive layer:
    // a lean or a hit reaction laid over whatever else plays. Built on first
    // use from the keys the import kept, which is why those are kept.
    const ozz::animation::Animation* Additive() const;

    // The clip with the root joint's horizontal travel and yaw taken out of
    // the animation and into tracks of their own, so a character that walks
    // forward in the clip stays put on the spot, and the distance it would
    // have covered is read off the tracks and handed to whatever moves the
    // entity. Built on first use; null when the joint does not move.
    struct RootMotion
    {
        ozz::unique_ptr<ozz::animation::Animation> mAnimation;
        ozz::unique_ptr<ozz::animation::Float3Track> mPosition;
        ozz::unique_ptr<ozz::animation::QuaternionTrack> mRotation;
        // The root's parent at rest, in model space: the tracks are in the
        // root's local space, and its parent may be an axis conversion.
        glm::quat mParentRotation = glm::quat( 1.0f, 0.0f, 0.0f, 0.0f );
        vec3 mParentScale = vec3( 1.0f );

        RootMotion();
        ~RootMotion();
        // The root's position and rotation at `ratio`, in model space.
        void Sample( f32 ratio, vec3& position, glm::quat& rotation ) const;
        // What the root travelled from `before` to `now`, around the loop if
        // `wrapped`; model space.
        void Delta( f32 before, f32 now, bool wrapped, vec3& translation, glm::quat& rotation ) const;
    };
    const RootMotion* WithRootMotion( const Skeleton& skeleton, i32 rootJoint ) const;

public:
    string mName;
    f32 mDuration = 0.0f;
    ozz::unique_ptr<ozz::animation::Animation> mAnimation;
    Scope<ozz::animation::offline::RawAnimation> mRaw;

private:
    mutable ozz::unique_ptr<ozz::animation::Animation> mAdditive;
    mutable bool mAdditiveBuilt = false;
    mutable Scope<RootMotion> mRootMotion;
    mutable i32 mRootMotionJoint = -1;
    mutable bool mRootMotionBuilt = false;
};

}
