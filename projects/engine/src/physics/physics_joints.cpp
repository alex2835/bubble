#include "physics_joints.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

namespace bubble::physics
{
namespace
{
constexpr f32 cPi = 3.14159265358979f;

// Jolt takes a joint's places from the body's centre of mass, not its
// origin; the world has neither.
JPH::Vec3 FromMass( const JPH::Body& body, vec3 point )
{
    if ( &body == &JPH::Body::sFixedToWorld or body.GetShape() == nullptr )
        return ToJolt( point );
    return ToJolt( point ) - body.GetShape()->GetCenterOfMass();
}

bool Limited( const Joint& joint )
{
    return joint.mMin < joint.mMax;
}

// A limit that gives, or a rigid one.
JPH::SpringSettings Spring( const Joint& joint )
{
    return JPH::SpringSettings( JPH::ESpringMode::FrequencyAndDamping, max( joint.mSpring, 0.0f ),
                                max( joint.mDamping, 0.0f ) );
}

JPH::MotorSettings Motor( const Joint& joint )
{
    JPH::MotorSettings motor;
    const f32 most = max( joint.mMaxForce, 0.0f );
    motor.SetForceLimit( most );
    motor.SetTorqueLimit( most );
    // A position motor pulls as a spring of 2 a second, settling at once.
    motor.mSpringSettings = JPH::SpringSettings( JPH::ESpringMode::FrequencyAndDamping, 2.0f, 1.0f );
    return motor;
}

JPH::EMotorState MotorState( const Joint& joint )
{
    if ( joint.mMotor == "velocity"_n )
        return JPH::EMotorState::Velocity;
    if ( joint.mMotor == "position"_n )
        return JPH::EMotorState::Position;
    return JPH::EMotorState::Off;
}
}

JointFrame Transformed( const mat4& pose, const JointFrame& frame )
{
    const mat3 turn( pose );
    JointFrame moved;
    moved.mPoint = vec3( pose * vec4( frame.mPoint, 1.0f ) );
    moved.mAxis = glm::normalize( turn * frame.mAxis );
    const vec3 normal = turn * frame.mNormal;
    moved.mNormal = glm::normalize( normal - moved.mAxis * glm::dot( normal, moved.mAxis ) );
    return moved;
}

expected<JPH::Ref<JPH::TwoBodyConstraint>, string> TryMakeConstraint( const Joint& joint,
                                                                      JPH::Body& first,
                                                                      const JointFrame& inFirst,
                                                                      JPH::Body& second,
                                                                      const JointFrame& inSecond )
{
    JPH::Ref<JPH::TwoBodyConstraintSettings> settings;
    if ( joint.mKind == "fixed"_n )
    {
        auto fixed = new JPH::FixedConstraintSettings();
        fixed->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        fixed->mPoint1 = FromMass( first, inFirst.mPoint );
        fixed->mAxisX1 = ToJolt( inFirst.mAxis );
        fixed->mAxisY1 = ToJolt( inFirst.mNormal );
        fixed->mPoint2 = FromMass( second, inSecond.mPoint );
        fixed->mAxisX2 = ToJolt( inSecond.mAxis );
        fixed->mAxisY2 = ToJolt( inSecond.mNormal );
        settings = fixed;
    }
    else if ( joint.mKind == "hinge"_n )
    {
        auto hinge = new JPH::HingeConstraintSettings();
        hinge->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        hinge->mPoint1 = FromMass( first, inFirst.mPoint );
        hinge->mHingeAxis1 = ToJolt( inFirst.mAxis );
        hinge->mNormalAxis1 = ToJolt( inFirst.mNormal );
        hinge->mPoint2 = FromMass( second, inSecond.mPoint );
        hinge->mHingeAxis2 = ToJolt( inSecond.mAxis );
        hinge->mNormalAxis2 = ToJolt( inSecond.mNormal );
        if ( Limited( joint ) )
        {
            hinge->mLimitsMin = clamp( joint.mMin, -cPi, 0.0f );
            hinge->mLimitsMax = clamp( joint.mMax, 0.0f, cPi );
        }
        hinge->mLimitsSpringSettings = Spring( joint );
        hinge->mMotorSettings = Motor( joint );
        settings = hinge;
    }
    else if ( joint.mKind == "slider"_n )
    {
        auto slider = new JPH::SliderConstraintSettings();
        slider->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        slider->mPoint1 = FromMass( first, inFirst.mPoint );
        slider->mSliderAxis1 = ToJolt( inFirst.mAxis );
        slider->mNormalAxis1 = ToJolt( inFirst.mNormal );
        slider->mPoint2 = FromMass( second, inSecond.mPoint );
        slider->mSliderAxis2 = ToJolt( inSecond.mAxis );
        slider->mNormalAxis2 = ToJolt( inSecond.mNormal );
        if ( Limited( joint ) )
        {
            slider->mLimitsMin = min( joint.mMin, 0.0f );
            slider->mLimitsMax = max( joint.mMax, 0.0f );
        }
        slider->mLimitsSpringSettings = Spring( joint );
        slider->mMotorSettings = Motor( joint );
        settings = slider;
    }
    else if ( joint.mKind == "distance"_n )
    {
        auto distance = new JPH::DistanceConstraintSettings();
        distance->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        distance->mPoint1 = FromMass( first, inFirst.mPoint );
        distance->mPoint2 = FromMass( second, inSecond.mPoint );
        // As far as the ends are now when no length is given: a rod.
        f32 low = joint.mMin;
        f32 high = joint.mMax;
        if ( low == 0.0f and high == 0.0f )
        {
            const JPH::RVec3 a = first.GetCenterOfMassTransform() * distance->mPoint1;
            const JPH::RVec3 b = second.GetCenterOfMassTransform() * distance->mPoint2;
            low = high = static_cast<f32>( ( a - b ).Length() );
        }
        if ( low > high or low < 0.0f )
            return unexpected( format( "joint: a distance from {} to {}: min from 0 up to max", low, high ) );
        distance->mMinDistance = low;
        distance->mMaxDistance = high;
        distance->mLimitsSpringSettings = Spring( joint );
        settings = distance;
    }
    else if ( joint.mKind == "swing_twist"_n )
    {
        auto swing = new JPH::SwingTwistConstraintSettings();
        swing->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        swing->mPosition1 = FromMass( first, inFirst.mPoint );
        swing->mTwistAxis1 = ToJolt( inFirst.mAxis );
        swing->mPlaneAxis1 = ToJolt( inFirst.mNormal );
        swing->mPosition2 = FromMass( second, inSecond.mPoint );
        swing->mTwistAxis2 = ToJolt( inSecond.mAxis );
        swing->mPlaneAxis2 = ToJolt( inSecond.mNormal );
        const f32 cone = clamp( joint.mSwing, 0.0f, cPi );
        swing->mNormalHalfConeAngle = cone;
        swing->mPlaneHalfConeAngle = cone;
        swing->mTwistMinAngle = Limited( joint ) ? clamp( joint.mMin, -cPi, 0.0f ) : -cPi;
        swing->mTwistMaxAngle = Limited( joint ) ? clamp( joint.mMax, 0.0f, cPi ) : cPi;
        swing->mSwingMotorSettings = Motor( joint );
        swing->mTwistMotorSettings = Motor( joint );
        settings = swing;
    }
    else
        return unexpected(
            format( "joint.kind: {} is none of fixed, hinge, slider, distance, swing_twist", joint.mKind.ToString() ) );

    settings->mEnabled = true;
    JPH::Ref<JPH::TwoBodyConstraint> constraint = settings->Create( first, second );
    ApplyMotor( joint, *constraint );
    return constraint;
}

void ApplyMotor( const Joint& joint, JPH::TwoBodyConstraint& constraint )
{
    const JPH::EMotorState state = MotorState( joint );
    const bool position = state == JPH::EMotorState::Position;
    switch ( constraint.GetSubType() )
    {
        case JPH::EConstraintSubType::Hinge:
        {
            auto& hinge = static_cast<JPH::HingeConstraint&>( constraint );
            hinge.GetMotorSettings() = Motor( joint );
            hinge.SetMotorState( state );
            if ( position )
                hinge.SetTargetAngle( joint.mTarget );
            else
                hinge.SetTargetAngularVelocity( joint.mTarget );
            break;
        }
        case JPH::EConstraintSubType::Slider:
        {
            auto& slider = static_cast<JPH::SliderConstraint&>( constraint );
            slider.GetMotorSettings() = Motor( joint );
            slider.SetMotorState( state );
            if ( position )
                slider.SetTargetPosition( joint.mTarget );
            else
                slider.SetTargetVelocity( joint.mTarget );
            break;
        }
        case JPH::EConstraintSubType::SwingTwist:
        {
            // The twist alone is driven; the swing is left free in its cone.
            auto& swing = static_cast<JPH::SwingTwistConstraint&>( constraint );
            swing.GetTwistMotorSettings() = Motor( joint );
            swing.SetSwingMotorState( JPH::EMotorState::Off );
            swing.SetTwistMotorState( state );
            if ( position )
                swing.SetTargetOrientationCS( JPH::Quat::sRotation( JPH::Vec3::sAxisX(), joint.mTarget ) );
            else
                swing.SetTargetAngularVelocityCS( JPH::Vec3( joint.mTarget, 0.0f, 0.0f ) );
            break;
        }
        default: break;
    }
}

f32 HeldForce( const JPH::TwoBodyConstraint& constraint, f32 dt )
{
    // The impulse that kept the anchors together, by the step's length.
    f32 impulse = 0.0f;
    switch ( constraint.GetSubType() )
    {
        case JPH::EConstraintSubType::Fixed:
            impulse = static_cast<const JPH::FixedConstraint&>( constraint ).GetTotalLambdaPosition().Length();
            break;
        case JPH::EConstraintSubType::Hinge:
            impulse = static_cast<const JPH::HingeConstraint&>( constraint ).GetTotalLambdaPosition().Length();
            break;
        case JPH::EConstraintSubType::Slider:
        {
            const JPH::Vector<2> along =
                static_cast<const JPH::SliderConstraint&>( constraint ).GetTotalLambdaPosition();
            impulse = std::sqrt( along[0] * along[0] + along[1] * along[1] );
            break;
        }
        case JPH::EConstraintSubType::Distance:
            impulse = std::abs( static_cast<const JPH::DistanceConstraint&>( constraint ).GetTotalLambdaPosition() );
            break;
        case JPH::EConstraintSubType::SwingTwist:
            impulse = static_cast<const JPH::SwingTwistConstraint&>( constraint ).GetTotalLambdaPosition().Length();
            break;
        default: break;
    }
    return dt > 0.0f ? impulse / dt : 0.0f;
}
}
