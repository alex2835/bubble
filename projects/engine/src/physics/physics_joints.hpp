#pragma once
// A joint component made Jolt's constraint: the frames it holds in each
// body, its kind's settings, its motor, what it bore last step.
#include "bubble/physics/physics.hpp"
#include "bubble/types/utility.hpp"
#include "jolt.hpp"

namespace bubble::physics
{
// Where a joint holds a body and which way, in the body's own space - its
// origin's, unscaled, as Jolt places the body.
struct JointFrame
{
    vec3 mPoint{ 0.0f };
    vec3 mAxis{ 1.0f, 0.0f, 0.0f };
    // Square to the axis: where a hinge's angle and a slider's turn are
    // measured from.
    vec3 mNormal{ 0.0f, 1.0f, 0.0f };

    bool operator==( const JointFrame& ) const = default;
};

// `frame` as `pose` - a body's world pose, or another frame's space - puts it.
JointFrame Transformed( const mat4& pose, const JointFrame& frame );

// The kind's constraint between `first` and `second` - Body::sFixedToWorld
// for the world -, each held at its frame; its motor set going.
expected<JPH::Ref<JPH::TwoBodyConstraint>, string> TryMakeConstraint( const Joint& joint,
                                                                      JPH::Body& first,
                                                                      const JointFrame& inFirst,
                                                                      JPH::Body& second,
                                                                      const JointFrame& inSecond );

// The motor as `joint` has it now: off, a velocity, a position.
void ApplyMotor( const Joint& joint, JPH::TwoBodyConstraint& constraint );

// The force that held the bodies together in the last step of `dt`.
f32 HeldForce( const JPH::TwoBodyConstraint& constraint, f32 dt );
}
