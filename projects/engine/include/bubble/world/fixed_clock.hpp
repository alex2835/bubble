#pragma once
#include "bubble/types/number.hpp"

namespace bubble
{
// Turns the time frames take, which varies, into whole steps of one length:
// logic and physics see the same dt at any frame rate, on any machine, and
// a replay comes out the same.
class FixedClock
{
public:
    // The most of one frame's time that counts: a long frame - a hitch, a
    // breakpoint - slows the game down rather than running a hundred steps
    // at once to catch up.
    static constexpr f64 cMostPerFrame = 0.25;

    // Steps per second; 0 is a logic_error.
    explicit FixedClock( u32 stepsPerSecond );

    // Takes a frame's time in seconds and returns how many steps are due.
    // Negative or NaN time is a logic_error.
    u32 Advance( f64 elapsed );

    // A step's length in seconds.
    f32 Dt() const { return static_cast<f32>( mDt ); }
    // How far the frame is between the last step and the next, in [0, 1):
    // what drawing interpolates by.
    f32 Alpha() const { return static_cast<f32>( mAccumulated / mDt ); }

private:
    f64 mDt = 0;
    // Time not yet spent on a step, always under mDt.
    f64 mAccumulated = 0;
};
}
