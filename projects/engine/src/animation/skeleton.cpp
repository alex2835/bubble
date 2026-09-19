#include "engine/pch/pch.hpp"
#include "engine/animation/skeleton.hpp"
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/additive_animation_builder.h>
#include <ozz/animation/offline/animation_builder.h>

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

const ozz::animation::Animation* AnimationClip::Additive() const
{
    if ( mAdditiveBuilt )
        return mAdditive.get();
    mAdditiveBuilt = true;
    if ( not mRaw )
        return nullptr;

    ozz::animation::offline::RawAnimation delta;
    if ( not ozz::animation::offline::AdditiveAnimationBuilder()( *mRaw, &delta ) )
    {
        LogError( "Animation '{}': failed to build the additive version", mName );
        return nullptr;
    }
    mAdditive = ozz::animation::offline::AnimationBuilder()( delta );
    if ( not mAdditive )
        LogError( "Animation '{}': failed to build the additive version", mName );
    return mAdditive.get();
}

}
