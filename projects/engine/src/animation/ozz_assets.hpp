// Skeletons and clips as the animation module samples them: ozz's types,
// seen nowhere outside src/animation.
#pragma once
#include "bubble/animation/animation_assets.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/utility.hpp"
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/track.h>

namespace bubble
{
struct SkeletonAsset
{
    Ref<const ozz::animation::Skeleton> mSkeleton;
};

struct ClipAsset
{
    string mName;
    f32 mDuration = 0;
    Ref<const ozz::animation::Animation> mAnimation;
    // Where its root went, in the model's space; none without root motion.
    Ref<const ozz::animation::Float3Track> mRootTranslation;
    Ref<const ozz::animation::QuaternionTrack> mRootRotation;
};

struct ClipsAsset
{
    vector<ClipAsset> mClips;

    opt<u32> TryFind( string_view name ) const;
    // For messages: "idle, walk", or "none".
    string Names() const;
};
}
