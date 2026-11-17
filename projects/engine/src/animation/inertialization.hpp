// Transitions eased by inertialization: the animation module's own.
#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/matrix.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// One joint's transform in its parent's space, the way the inertializer
// sees it: a struct a joint rather than ozz's four - the maths is per joint
// and per component, and a character has dozens of joints, not thousands.
struct JointPose
{
    vec3 mTranslation{ 0.0f };
    quat mRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 mScale{ 1.0f };
};

// Inertialization - Bollo, "High Performance Animation Transitions in Gears
// of War", GDC 2018 - in place of a cross fade. A cross fade plays the old
// state on for the whole blend and mixes the two; this remembers, at the
// switch, how far the pose is from the new state and how fast it moves,
// and lets that offset die away over the blend's time on a quintic that
// lands with no speed and no acceleration. Only the new state is sampled
// from then on, and a transition cut short by another starts a new decay
// from wherever the pose is, so interruptions never pop.
//
// Each part - translation, rotation, scale - of each joint is a number along
// a direction fixed at the switch: the offset's own direction, or the axis
// of the turn between them; the speed is measured along it.
class Inertializer
{
public:
    // `previous` and `beforePrevious` are the poses of the last two frames
    // as they were shown, `dt` the time between them; `target` the new
    // state's pose now. No duration, or no history, is a cut.
    void Begin( span<const JointPose> previous,
                span<const JointPose> beforePrevious,
                f32 dt,
                span<const JointPose> target,
                f32 duration );
    // Lays what is left of the offset on `pose` - the new state's, now - and
    // goes on by dt. Nothing once the transition has run out.
    void Apply( span<JointPose> pose, f32 dt );
    bool Active() const { return mTime < mDuration; }

    // The quintic of one number.
    struct Curve
    {
        f32 mX0 = 0;
        f32 mV0 = 0;
        f32 mA0 = 0;
        f32 mA = 0;
        f32 mB = 0;
        f32 mC = 0;
        f32 mDuration = 0;

        // `x0` the offset at the start, `v0` its speed, `duration` the
        // longest the decay may take: cut short when the speed already
        // heads for the target, which keeps it from overshooting.
        static Curve Make( f32 x0, f32 v0, f32 duration );
        f32 Evaluate( f32 t ) const;
    };

private:
    struct Joint
    {
        vec3 mTranslationDirection{ 0.0f };
        Curve mTranslation;
        vec3 mRotationAxis{ 0.0f };
        Curve mRotation;
        vec3 mScaleDirection{ 0.0f };
        Curve mScale;
    };

    vector<Joint> mJoints;
    f32 mTime = 0;
    f32 mDuration = 0;
};
}
