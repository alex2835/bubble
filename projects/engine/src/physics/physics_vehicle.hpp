#pragma once
// A vehicle component and its wheels made Jolt's vehicle constraint, and
// what the wheels' children are shown at.
#include "bubble/physics/physics.hpp"
#include "jolt.hpp"
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

namespace bubble::physics
{
// A wheel and where it is in its vehicle's body: its middle, all the way
// down, in the body's space.
struct WheelPlace
{
    VehicleWheel mWheel;
    vec3 mPosition{ 0.0f };
};

// The constraint on `body`, its wheels in order; not yet added to a system.
JPH::Ref<JPH::VehicleConstraint> MakeVehicle( const Vehicle& vehicle,
                                              span<const WheelPlace> wheels,
                                              JPH::Body& body,
                                              JPH::ObjectLayer layer );

// The driver's hands and feet, as the component has them now.
void Steer( const Vehicle& vehicle, JPH::VehicleConstraint& constraint );

// Wheel `index` - steered, turned, its suspension where it is - in its place's
// space: what its children hang from.
mat4 WheelOffset( const JPH::VehicleConstraint& constraint, u32 index, const WheelPlace& place );

VehicleGauges GaugesOf( const JPH::VehicleConstraint& constraint, const JPH::Body& body );
}
