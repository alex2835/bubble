 #pragma once
#include "engine/types/number.hpp"
#include "engine/types/string.hpp"
#include "engine/types/array.hpp"
#include "engine/types/map.hpp"
#include "engine/types/glm.hpp"
#include <ozz/base/memory/unique_ptr.h>
#include <optional>

namespace ozz::animation { class Skeleton; class Animation; }

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

public:
    string mName;
    f32 mDuration = 0.0f;
    ozz::unique_ptr<ozz::animation::Animation> mAnimation;
};

}
