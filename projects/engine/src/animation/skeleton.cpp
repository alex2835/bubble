#include "engine/pch/pch.hpp"
#include "engine/animation/skeleton.hpp"
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>

namespace bubble
{
// The special members live here so the header can forward declare the ozz
// types: ozz::unique_ptr's deleter needs the complete type where it is
// instantiated, and this is the one place that includes the runtime headers.
Skeleton::Skeleton() = default;
Skeleton::~Skeleton() = default;
Skeleton::Skeleton( Skeleton&& ) = default;
Skeleton& Skeleton::operator=( Skeleton&& ) = default;

u32 Skeleton::JointCount() const
{
    return mSkeleton ? static_cast<u32>( mSkeleton->num_joints() ) : 0;
}

std::optional<u16> Skeleton::JointIndex( string_view name ) const
{
    auto iter = mJointByName.find( name );
    if ( iter == mJointByName.end() )
        return std::nullopt;
    return iter->second;
}


AnimationClip::AnimationClip() = default;
AnimationClip::~AnimationClip() = default;
AnimationClip::AnimationClip( AnimationClip&& ) = default;
AnimationClip& AnimationClip::operator=( AnimationClip&& ) = default;

}
