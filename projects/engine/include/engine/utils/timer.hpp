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

    f32 Seconds();
    f32 Milliseconds();

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

    // Time from the program start
    static DeltaTime GetGlobalTime();
    static DeltaTime GetGlobalStartTime();
    static DeltaTime GetFromStartTime();

private:
    static TimePoint Now();
    static TimePoint mGlobalStartTime;

    TimePoint mLastTime = Now();
    DeltaTime mDeltatime;
};

}
