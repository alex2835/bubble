
#include "engine/utils/timer.hpp"

namespace bubble
{
std::time_point Timer::mGlobalStartTime = Now();


TimePoint::TimePoint( f32 time )
    : mTime( time )
{
}

f32 TimePoint::Seconds()
{
    return mTime;
}

f32 TimePoint::Milliseconds()
{
    return mTime * 1000.0f;
}



void Timer::OnUpdate()
{
    std::time_point now = Now();
    std::duration<f32> time_dif = duration_cast<std::duration<f32>>( now - mLastTime );
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
    return TimePoint( std::duration_cast<std::duration<f32>>( 
                            std::system_clock::now().time_since_epoch() ).count() );
}

}