#pragma once
#include "engine/types/number.hpp"
#include "engine/types/array.hpp"
#include "engine/types/glm.hpp"
#include <glm/gtc/quaternion.hpp>
#include <span>

namespace bubble
{
// One joint's local transform, the way the inertializer sees it. Array of
// structs rather than ozz's SoA: the maths is per joint and per component,
// and a character has a few dozen joints, not a few thousand.
struct JointPose
{
    vec3 mTranslation = vec3( 0.0f );
    glm::quat mRotation = glm::quat( 1.0f, 0.0f, 0.0f, 0.0f );
    vec3 mScale = vec3( 1.0f );
};


// Inertialization - Bollo, "High Performance Animation Transitions in Gears
// of War", GDC 2018 - in place of a cross fade.
//
// A cross fade keeps evaluating the outgoing animation for the length of the
// blend and mixes the two. This instead remembers, at the instant of the
// switch, how far the pose was from the new animation and how fast it was
// moving, and decays that offset to zero over the blend time with a quintic
// that also lands with zero velocity and acceleration. Only the new animation
// is evaluated from then on; the old one is gone the moment the switch
// happens. A transition interrupted by another simply starts a new decay
// from wherever the pose is, with whatever velocity it has, so interruptions
// cost nothing and never pop.
//
// Each component (translation, rotation, scale) of each joint is reduced to
// a scalar along a fixed direction: the offset's own direction, or the axis
// of the rotation offset. Velocity is projected onto that same direction.
class Inertializer
{
public:
    // Starts a transition. `previous` and `beforePrevious` are the poses of
    // the last two frames as they were finally shown, `dt` the time between
    // them; `target` is the new animation's pose for this frame. A duration
    // of zero (or no history) is a snap: nothing is recorded.
    void Begin( std::span<const JointPose> previous,
                std::span<const JointPose> beforePrevious,
                f32 dt,
                std::span<const JointPose> target,
                f32 duration );

    // Applies this frame's remaining offset onto `pose`, the new animation's
    // pose for this frame, and advances by dt. Does nothing once the
    // transition has run out.
    void Apply( std::span<JointPose> pose, f32 dt );

    bool Active() const { return mTime < mDuration; }
    // How far through, 0..1; 1 when idle.
    f32 Progress() const { return mDuration > 0.0f ? std::min( mTime / mDuration, 1.0f ) : 1.0f; }

    // The quintic for one scalar. Public for the tests.
    struct Curve
    {
        f32 mX0 = 0.0f, mV0 = 0.0f, mA0 = 0.0f;
        f32 mA = 0.0f, mB = 0.0f, mC = 0.0f;
        f32 mDuration = 0.0f;

        // x0 is the offset at t = 0, v0 its velocity, duration the longest
        // the decay may take - it is cut short when the velocity already
        // points at the target, which is what stops it overshooting.
        static Curve Make( f32 x0, f32 v0, f32 duration );
        f32 Evaluate( f32 t ) const;
    };

private:
    struct Joint
    {
        vec3 mTranslationDir = vec3( 0.0f );
        Curve mTranslation;
        vec3 mRotationAxis = vec3( 0.0f );
        Curve mRotation;
        vec3 mScaleDir = vec3( 0.0f );
        Curve mScale;
    };

    vector<Joint> mJoints;
    f32 mTime = 0.0f;
    f32 mDuration = 0.0f;
};

}
