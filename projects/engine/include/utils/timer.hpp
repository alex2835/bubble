#pragma once
#include <time.h>
#include <chrono>

namespace bubble
{
class Time
{
public:
    Time() = default;
    // Time in seconds
    Time( float time );

    float GetSeconds();
    float GetMilliseconds();

private:
    float mTime = 0.0f;
};
typedef Time DeltaTime;


class Timer
{
public:
    void Update();
    DeltaTime GetDeltaTime();

    // Time from the program start
    static Time GetTime();

private:
    static std::chrono::high_resolution_clock::time_point Now();
    std::chrono::high_resolution_clock::time_point mLastTime = Now();
    DeltaTime mDeltatime;
};

}
