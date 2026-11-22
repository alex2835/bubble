#include "physics_vehicle.hpp"
#include "bubble/types/algorithm.hpp"
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

namespace bubble::physics
{
namespace
{
// The engine's forward and up, in the body's space.
const JPH::Vec3 cForward( 0.0f, 0.0f, -1.0f );
const JPH::Vec3 cUp( 0.0f, 1.0f, 0.0f );

// A road tyre's friction, by Jolt's sample, scaled by the wheel's grip.
void Tyres( JPH::WheelSettingsWV& settings, f32 grip )
{
    settings.mLongitudinalFriction.Clear();
    settings.mLongitudinalFriction.Reserve( 3 );
    settings.mLongitudinalFriction.AddPoint( 0.0f, 0.0f );
    settings.mLongitudinalFriction.AddPoint( 0.06f, 1.2f * grip );
    settings.mLongitudinalFriction.AddPoint( 0.2f, 1.0f * grip );
    settings.mLateralFriction.Clear();
    settings.mLateralFriction.Reserve( 3 );
    settings.mLateralFriction.AddPoint( 0.0f, 0.0f );
    settings.mLateralFriction.AddPoint( 3.0f, 1.2f * grip );
    settings.mLateralFriction.AddPoint( 20.0f, 1.0f * grip );
}
}

JPH::Ref<JPH::VehicleConstraint> MakeVehicle( const Vehicle& vehicle,
                                              span<const WheelPlace> wheels,
                                              JPH::Body& body,
                                              JPH::ObjectLayer layer )
{
    JPH::VehicleConstraintSettings settings;
    settings.mUp = cUp;
    settings.mForward = cForward;
    settings.mMaxPitchRollAngle = clamp( vehicle.mMaxTilt, 0.0f, 3.14159265f );
    for ( const WheelPlace& place : wheels )
    {
        const VehicleWheel& wheel = place.mWheel;
        JPH::Ref<JPH::WheelSettingsWV> made = new JPH::WheelSettingsWV();
        const f32 travel = max( wheel.mSuspension, 0.01f );
        // Hung from above its place by its travel, down to its place.
        made->mPosition = ToJolt( place.mPosition ) + cUp * travel;
        made->mSuspensionDirection = -cUp;
        made->mSteeringAxis = cUp;
        made->mWheelUp = cUp;
        made->mWheelForward = cForward;
        made->mSuspensionMinLength = 0.0f;
        made->mSuspensionMaxLength = travel;
        made->mSuspensionSpring = JPH::SpringSettings( JPH::ESpringMode::FrequencyAndDamping,
                                                       max( wheel.mStiffness, 0.01f ), max( wheel.mDamping, 0.0f ) );
        made->mRadius = max( wheel.mRadius, 0.01f );
        made->mWidth = max( wheel.mWidth, 0.01f );
        // A wheel of 20 kilograms, as a solid disc.
        made->mInertia = 0.5f * 20.0f * made->mRadius * made->mRadius;
        made->mMaxSteerAngle = max( wheel.mMaxSteer, 0.0f );
        made->mMaxBrakeTorque = max( wheel.mBrakeTorque, 0.0f );
        made->mMaxHandBrakeTorque = wheel.mHandbrake ? 4000.0f : 0.0f;
        Tyres( *made, max( wheel.mGrip, 0.0f ) );
        settings.mWheels.push_back( JPH::Ref<JPH::WheelSettings>( made.GetPtr() ) );
    }

    JPH::Ref<JPH::WheeledVehicleControllerSettings> controller = new JPH::WheeledVehicleControllerSettings();
    controller->mEngine.mMaxTorque = max( vehicle.mEngineTorque, 0.0f );
    controller->mEngine.mMaxRPM = max( vehicle.mMaxRpm, controller->mEngine.mMinRPM + 1.0f );
    // The driven wheels by axle - those as far forward as each other -, left
    // and right by their side; each axle an equal share of the engine.
    vector<u32> driven;
    for ( u32 index = 0; index < wheels.size(); ++index )
        if ( wheels[index].mWheel.mDrive )
            driven.push_back( index );
    ranges::stable_sort( driven, [&]( u32 a, u32 b ) { return wheels[a].mPosition.z < wheels[b].mPosition.z; } );
    for ( size_t at = 0; at < driven.size(); )
    {
        JPH::VehicleDifferentialSettings axle;
        const f32 z = wheels[driven[at]].mPosition.z;
        for ( ; at < driven.size() and std::abs( wheels[driven[at]].mPosition.z - z ) < 0.05f; ++at )
        {
            const int index = static_cast<int>( driven[at] );
            int& side = wheels[driven[at]].mPosition.x < 0.0f ? axle.mLeftWheel : axle.mRightWheel;
            // A third wheel on an axle is left out of the engine's.
            if ( side < 0 )
                side = index;
        }
        // One wheel alone takes the whole axle's share.
        if ( axle.mLeftWheel < 0 or axle.mRightWheel < 0 )
            axle.mLeftRightSplit = axle.mLeftWheel < 0 ? 1.0f : 0.0f;
        controller->mDifferentials.push_back( axle );
    }
    for ( JPH::VehicleDifferentialSettings& axle : controller->mDifferentials )
        axle.mEngineTorqueRatio = 1.0f / static_cast<f32>( controller->mDifferentials.size() );
    settings.mController = controller;

    JPH::Ref<JPH::VehicleConstraint> constraint = new JPH::VehicleConstraint( body, settings );
    constraint->SetVehicleCollisionTester( new JPH::VehicleCollisionTesterCastCylinder( layer ) );
    Steer( vehicle, *constraint );
    return constraint;
}

void Steer( const Vehicle& vehicle, JPH::VehicleConstraint& constraint )
{
    auto& controller = static_cast<JPH::WheeledVehicleController&>( *constraint.GetController() );
    controller.SetDriverInput( clamp( vehicle.mThrottle, -1.0f, 1.0f ), clamp( vehicle.mSteer, -1.0f, 1.0f ),
                               clamp( vehicle.mBrake, 0.0f, 1.0f ), clamp( vehicle.mHandbrake, 0.0f, 1.0f ) );
}

mat4 WheelOffset( const JPH::VehicleConstraint& constraint, u32 index, const WheelPlace& place )
{
    // The wheel's model space - its axle along X - in the body's.
    const JPH::Mat44 wheel =
        constraint.GetWheelLocalTransform( index, JPH::Vec3( 1.0f, 0.0f, 0.0f ), JPH::Vec3( 0.0f, 1.0f, 0.0f ) );
    mat4 inBody( 1.0f );
    for ( int column = 0; column < 4; ++column )
    {
        const JPH::Vec4 values = wheel.GetColumn4( column );
        inBody[column] = vec4( values.GetX(), values.GetY(), values.GetZ(), values.GetW() );
    }
    // From its place, which faces as the body does.
    return glm::translate( mat4( 1.0f ), -place.mPosition ) * inBody;
}

VehicleGauges GaugesOf( const JPH::VehicleConstraint& constraint, const JPH::Body& body )
{
    const auto& controller = static_cast<const JPH::WheeledVehicleController&>( *constraint.GetController() );
    const JPH::Vec3 forward = body.GetRotation() * cForward;
    return VehicleGauges{ .mSpeed = body.GetLinearVelocity().Dot( forward ),
                          .mRpm = controller.GetEngine().GetCurrentRPM(),
                          .mGear = controller.GetTransmission().GetCurrentGear() };
}
}
