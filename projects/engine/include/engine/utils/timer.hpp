#pragma once
#include "engine/utils/chrono.hpp"
#include "engine/types/number.hpp"

namespace bubble
{
class DeltaTime
{
public:
    DeltaTime() = default;
    // Time in seconds
    explicit DeltaTime( f32 time );

    f32 Seconds() const;
    f32 Milliseconds() const;

    // second
    operator float() { return mTime; }

private:
    f32 mTime = 0.0f;
};


class Timer
{
public:
    void OnUpdate();
    DeltaTime GetDeltaTime();

    // Zero the play clock and forget the last frame. The second half matters as
    // much as the first: OnUpdate only runs while the engine is playing, so
    // without this the first frame after pressing play reports a delta covering
    // however long the editor sat in edit mode, and every script sees it.
    void Reset();

    // Seconds since Reset(). This is the clock scripts get as time() - it starts
    // at zero on play, unlike GetGlobalTime which is seconds since the epoch and
    // has roughly 128 second resolution once squeezed into an f32.
    DeltaTime GetElapsed() const;

    // Time from the program start
    static DeltaTime GetGlobalTime();
    static DeltaTime GetGlobalStartTime();
    static DeltaTime GetFromStartTime();

private:
    static TimePoint Now();
    static TimePoint mGlobalStartTime;

    TimePoint mLastTime = Now();
    DeltaTime mDeltatime;
    DeltaTime mElapsed;
};

}
