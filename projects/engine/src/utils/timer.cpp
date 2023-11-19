
#include "utils/timer.hpp"
using namespace std::chrono;

namespace bubble
{

Time::Time( float time )
    : mTime( time )
{
}

float Time::GetSeconds()
{
    return mTime;
}

float Time::GetMilliseconds()
{
    return mTime * 1000.0f;
}

void Timer::Update()
{
    high_resolution_clock::time_point now = Now();
    duration<float> time_dif = duration_cast<duration<float>>( now - mLastTime );
    mDeltatime = time_dif.count();
    mLastTime = now;
}

DeltaTime Timer::GetDeltaTime()
{
    return mDeltatime;
}

std::chrono::high_resolution_clock::time_point Timer::Now()
{
    return  high_resolution_clock::now();
}

//Time Timer::GetTime()
//{
//    high_resolution_clock::time_point now = Now();
//    duration<float> time_dif = duration_cast<duration<float>>( now - ProgramStartTime );
//    return time_dif.count();
//}

}