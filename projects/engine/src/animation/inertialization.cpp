#include "engine/pch/pch.hpp"
#include "engine/animation/inertialization.hpp"

namespace bubble
{
namespace
{
constexpr f32 cEpsilon = 1e-6f;

// The offset of `from` relative to `to`, as a scalar along `dir`: the length
// when dir is the offset's own direction, its projection otherwise.
f32 Along( const vec3& from, const vec3& to, const vec3& dir )
{
    return glm::dot( from - to, dir );
}

// The rotation taking `to` onto `from`, in the parent's frame, as an angle
// about `axis`. The sign is a projection of the rotation onto the axis, so
// the velocity of an offset can be measured along the axis of the offset
// itself.
f32 AngleAbout( const glm::quat& from, const glm::quat& to, const vec3& axis )
{
    glm::quat q = from * glm::inverse( to );
    if ( q.w < 0.0f )
        q = -q;
    return 2.0f * std::atan2( glm::dot( vec3( q.x, q.y, q.z ), axis ), q.w );
}
}


Inertializer::Curve Inertializer::Curve::Make( f32 x0, f32 v0, f32 duration )
{
    Curve curve;
    curve.mX0 = x0;
    curve.mV0 = v0;
    if ( duration <= cEpsilon or ( std::abs( x0 ) <= cEpsilon and std::abs( v0 ) <= cEpsilon ) )
        return curve;

    // Already moving towards the target: let it arrive on its own rather than
    // curve past it and come back. -5 x0 / v0 is where a quintic starting
    // with this velocity would first cross zero.
    if ( v0 * x0 < 0.0f )
        duration = std::min( duration, -5.0f * x0 / v0 );
    if ( duration <= cEpsilon )
        return curve;

    const f32 t1 = duration;
    const f32 t2 = t1 * t1;
    const f32 t3 = t2 * t1;
    const f32 t4 = t3 * t1;
    const f32 t5 = t4 * t1;

    // Constrained to reach zero offset, velocity and acceleration at t1. The
    // starting acceleration is the free parameter, chosen to keep the curve
    // as gentle as those constraints allow.
    const f32 a0 = ( -8.0f * v0 * t1 - 20.0f * x0 ) / t2;
    curve.mA0 = a0;
    curve.mA = -( a0 * t2 + 6.0f * v0 * t1 + 12.0f * x0 ) / ( 2.0f * t5 );
    curve.mB = ( 3.0f * a0 * t2 + 16.0f * v0 * t1 + 30.0f * x0 ) / ( 2.0f * t4 );
    curve.mC = -( 3.0f * a0 * t2 + 12.0f * v0 * t1 + 20.0f * x0 ) / ( 2.0f * t3 );
    curve.mDuration = t1;
    return curve;
}

f32 Inertializer::Curve::Evaluate( f32 t ) const
{
    if ( t >= mDuration )
        return 0.0f;
    if ( t <= 0.0f )
        return mX0;
    const f32 t2 = t * t;
    const f32 t3 = t2 * t;
    const f32 t4 = t3 * t;
    const f32 t5 = t4 * t;
    return mA * t5 + mB * t4 + mC * t3 + 0.5f * mA0 * t2 + mV0 * t + mX0;
}


void Inertializer::Begin( std::span<const JointPose> previous,
                          std::span<const JointPose> beforePrevious,
                          f32 dt,
                          std::span<const JointPose> target,
                          f32 duration )
{
    mTime = 0.0f;
    mDuration = 0.0f;
    if ( duration <= cEpsilon or previous.size() != target.size() )
        return;

    // Without a frame before the previous one there is no velocity; the
    // offset still decays, it just starts from rest.
    const bool haveVelocity = beforePrevious.size() == target.size() and dt > cEpsilon;
    const auto velocity = [&]( f32 x0, f32 xBefore ) { return haveVelocity ? ( x0 - xBefore ) / dt : 0.0f; };

    mJoints.resize( target.size() );
    for ( size_t j = 0; j < target.size(); j++ )
    {
        Joint& joint = mJoints[j];
        const JointPose& prev = previous[j];
        const JointPose& before = haveVelocity ? beforePrevious[j] : prev;
        const JointPose& to = target[j];

        {
            const vec3 offset = prev.mTranslation - to.mTranslation;
            const f32 x0 = glm::length( offset );
            joint.mTranslationDir = x0 > cEpsilon ? offset / x0 : vec3( 0.0f );
            const f32 xBefore = Along( before.mTranslation, to.mTranslation, joint.mTranslationDir );
            joint.mTranslation = Curve::Make( x0, velocity( x0, xBefore ), duration );
        }
        {
            glm::quat q0 = prev.mRotation * glm::inverse( to.mRotation );
            if ( q0.w < 0.0f )
                q0 = -q0;
            const vec3 axis = vec3( q0.x, q0.y, q0.z );
            const f32 sine = glm::length( axis );
            joint.mRotationAxis = sine > cEpsilon ? axis / sine : vec3( 0.0f );
            const f32 x0 = 2.0f * std::atan2( sine, q0.w );
            const f32 xBefore = AngleAbout( before.mRotation, to.mRotation, joint.mRotationAxis );
            joint.mRotation = Curve::Make( x0, velocity( x0, xBefore ), duration );
        }
        {
            const vec3 offset = prev.mScale - to.mScale;
            const f32 x0 = glm::length( offset );
            joint.mScaleDir = x0 > cEpsilon ? offset / x0 : vec3( 0.0f );
            const f32 xBefore = Along( before.mScale, to.mScale, joint.mScaleDir );
            joint.mScale = Curve::Make( x0, velocity( x0, xBefore ), duration );
        }
    }
    mDuration = duration;
}


void Inertializer::Apply( std::span<JointPose> pose, f32 dt )
{
    if ( not Active() or pose.size() != mJoints.size() )
        return;

    for ( size_t j = 0; j < pose.size(); j++ )
    {
        const Joint& joint = mJoints[j];
        JointPose& p = pose[j];

        p.mTranslation += joint.mTranslationDir * joint.mTranslation.Evaluate( mTime );
        p.mScale += joint.mScaleDir * joint.mScale.Evaluate( mTime );

        const f32 angle = joint.mRotation.Evaluate( mTime );
        if ( angle != 0.0f )
            p.mRotation = glm::normalize( glm::angleAxis( angle, joint.mRotationAxis ) * p.mRotation );
    }
    mTime += dt;
}

}
