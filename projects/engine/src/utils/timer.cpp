
#include "engine/utils/timer.hpp"

namespace bubble
{
std::time_point Timer::mGlobalStartTime = Now();


TimePoint::TimePoint( float time )
    : mTime( time )
{
}

float TimePoint::GetSeconds()
{
    return mTime;
}

float TimePoint::GetMilliseconds()
{
    return mTime * 1000.0f;
}



void Timer::OnUpdate()
{
    std::time_point now = Now();
    std::duration<float> time_dif = duration_cast<std::duration<float>>( now - mLastTime );
    mDeltatime = DeltaTime( time_dif.count() );
    mLastTime = now;
}

DeltaTime Timer::GetDeltaTime()
{
    return mDeltatime;
}

std::time_point Timer::Now()
{
    return  std::high_resolution_clock::now();
}

TimePoint Timer::GetGlobalTime()
{
    return TimePoint( std::duration_cast<std::duration<float>>( 
                            std::system_clock::now().time_since_epoch() ).count() );
}

}