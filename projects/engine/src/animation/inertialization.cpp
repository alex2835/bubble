#include "inertialization.hpp"
#include "bubble/types/algorithm.hpp"

namespace bubble
{
namespace
{
constexpr f32 cEpsilon = 1e-6f;

// The turn taking `to` onto `from` as an angle about `axis`: the speed of an
// offset is measured along the axis of the offset itself.
f32 AngleAbout( const quat& from, const quat& to, vec3 axis )
{
    quat turn = from * glm::inverse( to );
    if ( turn.w < 0 )
        turn = -turn;
    return 2.0f * atan2( glm::dot( vec3( turn.x, turn.y, turn.z ), axis ), turn.w );
}
}

Inertializer::Curve Inertializer::Curve::Make( f32 x0, f32 v0, f32 duration )
{
    Curve curve;
    curve.mX0 = x0;
    curve.mV0 = v0;
    if ( duration <= cEpsilon or ( abs( x0 ) <= cEpsilon and abs( v0 ) <= cEpsilon ) )
        return curve;
    // Heading for the target already: it arrives on its own rather than
    // curving past and back. -5 x0 / v0 is where a quintic starting at this
    // speed first meets zero.
    if ( v0 * x0 < 0 )
        duration = min( duration, -5.0f * x0 / v0 );
    if ( duration <= cEpsilon )
        return curve;
    const f32 t1 = duration;
    const f32 t2 = t1 * t1;
    const f32 t3 = t2 * t1;
    const f32 t4 = t3 * t1;
    const f32 t5 = t4 * t1;
    // No offset, speed or acceleration at t1; the starting acceleration is
    // the free one, as gentle as the rest allows.
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
        return 0;
    if ( t <= 0 )
        return mX0;
    const f32 t2 = t * t;
    const f32 t3 = t2 * t;
    const f32 t4 = t3 * t;
    const f32 t5 = t4 * t;
    return mA * t5 + mB * t4 + mC * t3 + 0.5f * mA0 * t2 + mV0 * t + mX0;
}

void Inertializer::Begin( span<const JointPose> previous,
                          span<const JointPose> beforePrevious,
                          f32 dt,
                          span<const JointPose> target,
                          f32 duration )
{
    mTime = 0;
    mDuration = 0;
    if ( duration <= cEpsilon or previous.size() != target.size() )
        return;
    // Without the frame before the last there is no speed: the offset still
    // dies away, from rest.
    const bool moving = beforePrevious.size() == target.size() and dt > cEpsilon;
    const auto speed = [&]( f32 x0, f32 xBefore ) { return moving ? ( x0 - xBefore ) / dt : 0.0f; };
    mJoints.resize( target.size() );
    for ( size_t j = 0; j < target.size(); ++j )
    {
        Joint& joint = mJoints[j];
        const JointPose& last = previous[j];
        const JointPose& before = moving ? beforePrevious[j] : last;
        const JointPose& to = target[j];
        {
            const vec3 offset = last.mTranslation - to.mTranslation;
            const f32 x0 = glm::length( offset );
            joint.mTranslationDirection = x0 > cEpsilon ? offset / x0 : vec3( 0.0f );
            const f32 xBefore = glm::dot( before.mTranslation - to.mTranslation, joint.mTranslationDirection );
            joint.mTranslation = Curve::Make( x0, speed( x0, xBefore ), duration );
        }
        {
            quat turn = last.mRotation * glm::inverse( to.mRotation );
            if ( turn.w < 0 )
                turn = -turn;
            const vec3 axis( turn.x, turn.y, turn.z );
            const f32 sine = glm::length( axis );
            joint.mRotationAxis = sine > cEpsilon ? axis / sine : vec3( 0.0f );
            const f32 x0 = 2.0f * atan2( sine, turn.w );
            const f32 xBefore = AngleAbout( before.mRotation, to.mRotation, joint.mRotationAxis );
            joint.mRotation = Curve::Make( x0, speed( x0, xBefore ), duration );
        }
        {
            const vec3 offset = last.mScale - to.mScale;
            const f32 x0 = glm::length( offset );
            joint.mScaleDirection = x0 > cEpsilon ? offset / x0 : vec3( 0.0f );
            const f32 xBefore = glm::dot( before.mScale - to.mScale, joint.mScaleDirection );
            joint.mScale = Curve::Make( x0, speed( x0, xBefore ), duration );
        }
    }
    mDuration = duration;
}

void Inertializer::Apply( span<JointPose> pose, f32 dt )
{
    if ( not Active() or pose.size() != mJoints.size() )
        return;
    for ( size_t j = 0; j < pose.size(); ++j )
    {
        const Joint& joint = mJoints[j];
        JointPose& p = pose[j];
        p.mTranslation += joint.mTranslationDirection * joint.mTranslation.Evaluate( mTime );
        p.mScale += joint.mScaleDirection * joint.mScale.Evaluate( mTime );
        const f32 angle = joint.mRotation.Evaluate( mTime );
        if ( angle != 0 )
            p.mRotation = glm::normalize( glm::angleAxis( angle, joint.mRotationAxis ) * p.mRotation );
    }
    mTime += dt;
}
}
