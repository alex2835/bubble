#include "engine/pch/pch.hpp"
#include "engine/animation/skeleton.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/additive_animation_builder.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/motion_extractor.h>
#include <ozz/animation/offline/raw_track.h>
#include <ozz/animation/offline/track_builder.h>
#include <ozz/animation/runtime/track.h>
#include <ozz/animation/runtime/track_sampling_job.h>
#include <ozz/animation/runtime/skeleton_utils.h>

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



AnimationClip::RootMotion::RootMotion() = default;
AnimationClip::RootMotion::~RootMotion() = default;

void AnimationClip::RootMotion::Sample( f32 ratio, vec3& position, glm::quat& rotation ) const
{
    ozz::math::Float3 p;
    ozz::animation::Float3TrackSamplingJob positionJob;
    positionJob.track = mPosition.get();
    positionJob.ratio = ratio;
    positionJob.result = &p;
    positionJob.Run();

    ozz::math::Quaternion q;
    ozz::animation::QuaternionTrackSamplingJob rotationJob;
    rotationJob.track = mRotation.get();
    rotationJob.ratio = ratio;
    rotationJob.result = &q;
    rotationJob.Run();

    position = mParentRotation * ( vec3( p.x, p.y, p.z ) * mParentScale );
    rotation = mParentRotation * glm::quat( q.w, q.x, q.y, q.z ) * glm::inverse( mParentRotation );
}

void AnimationClip::RootMotion::Delta( f32 before, f32 now, bool wrapped, vec3& translation, glm::quat& rotation ) const
{
    // Between two points on the track, as the difference of the samples;
    // around the seam, as the run to the end plus the run from the start.
    const auto between = [&]( f32 from, f32 to, vec3& t, glm::quat& r )
    {
        vec3 p0, p1;
        glm::quat q0, q1;
        Sample( from, p0, q0 );
        Sample( to, p1, q1 );
        t = p1 - p0;
        r = q1 * glm::inverse( q0 );
    };
    if ( not wrapped )
    {
        between( before, now, translation, rotation );
        return;
    }
    vec3 t0, t1;
    glm::quat r0, r1;
    if ( now < before )
    {
        between( before, 1.0f, t0, r0 );
        between( 0.0f, now, t1, r1 );
    }
    else
    {
        between( before, 0.0f, t0, r0 );
        between( 1.0f, now, t1, r1 );
    }
    translation = t0 + t1;
    rotation = r1 * r0;
}

const AnimationClip::RootMotion* AnimationClip::WithRootMotion( const Skeleton& skeleton, i32 rootJoint ) const
{
    if ( mRootMotionBuilt and mRootMotionJoint == rootJoint )
        return mRootMotion.get();
    mRootMotionBuilt = true;
    mRootMotionJoint = rootJoint;
    mRootMotion.reset();
    if ( not mRaw or not skeleton.mSkeleton or rootJoint < 0 or rootJoint >= skeleton.mSkeleton->num_joints() )
        return nullptr;

    // The tracks are the root's local transform, in its parent's space -
    // and the parent may be an axis conversion (a Z-up file's root under a
    // node that turns it Y-up), so "horizontal" and "yaw" have to be found
    // in that space: the local axis closest to the model's up is the one
    // the height and the yaw are about.
    auto motion = CreateScope<RootMotion>();
    const int parent = skeleton.mSkeleton->joint_parents()[rootJoint];
    if ( parent >= 0 )
    {
        const auto rest = ozz::animation::GetRestPoseModelSpace( *skeleton.mSkeleton );
        mat4 parentModel;
        memcpy( &parentModel, &rest[parent], sizeof( parentModel ) );
        vec3 scale, translation, skew;
        vec4 perspective;
        glm::quat orientation;
        if ( glm::decompose( parentModel, scale, orientation, translation, skew, perspective ) )
        {
            motion->mParentRotation = orientation;
            motion->mParentScale = scale;
        }
    }
    const vec3 localUp = glm::inverse( motion->mParentRotation ) * vec3( 0.0f, 1.0f, 0.0f );
    const int up = std::abs( localUp.x ) > std::abs( localUp.y ) and std::abs( localUp.x ) > std::abs( localUp.z ) ? 0
                 : std::abs( localUp.z ) > std::abs( localUp.y ) ? 2 : 1;

    // Horizontal travel and yaw, which is what locomotion carries; height
    // and the other rotations stay in the animation.
    ozz::animation::offline::MotionExtractor extractor;
    extractor.root_joint = rootJoint;
    extractor.position_settings = { up != 0, up != 1, up != 2, ozz::animation::offline::MotionExtractor::Reference::kSkeleton, true, false };
    extractor.rotation_settings = { up == 0, up == 1, up == 2, ozz::animation::offline::MotionExtractor::Reference::kSkeleton, true, false };

    ozz::animation::offline::RawFloat3Track position;
    ozz::animation::offline::RawQuaternionTrack rotation;
    ozz::animation::offline::RawAnimation baked;
    if ( not extractor( *mRaw, *skeleton.mSkeleton, &position, &rotation, &baked ) )
    {
        LogError( "Animation '{}': failed to extract root motion", mName );
        return nullptr;
    }
    motion->mAnimation = ozz::animation::offline::AnimationBuilder()( baked );
    motion->mPosition = ozz::animation::offline::TrackBuilder()( position );
    motion->mRotation = ozz::animation::offline::TrackBuilder()( rotation );
    if ( not motion->mAnimation or not motion->mPosition or not motion->mRotation )
    {
        LogError( "Animation '{}': failed to build the root motion version", mName );
        return nullptr;
    }

    mRootMotion = std::move( motion );
    return mRootMotion.get();
}

}
