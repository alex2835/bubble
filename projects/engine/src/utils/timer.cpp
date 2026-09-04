
#include "engine/utils/timer.hpp"

namespace bubble
{
TimePoint Timer::mGlobalStartTime = Now();


DeltaTime::DeltaTime( f32 time )
    : mTime( time )
{
}

f32 DeltaTime::Seconds() const
{
    return mTime;
}

f32 DeltaTime::Milliseconds() const
{
    return mTime * 1000.0f;
}



void Timer::OnUpdate()
{
    TimePoint now = Now();
    chrono::duration<f32> time_dif = duration_cast<chrono::duration<f32>>( now - mLastTime );
    mDeltatime = DeltaTime( time_dif.count() );
    mLastTime = now;
    mElapsed = DeltaTime( mElapsed.Seconds() + mDeltatime.Seconds() );
}

void Timer::Reset()
{
    mLastTime = Now();
    mDeltatime = DeltaTime( 0.0f );
    mElapsed = DeltaTime( 0.0f );
}

DeltaTime Timer::GetElapsed() const
{
    return mElapsed;
}

DeltaTime Timer::GetDeltaTime()
{
    return mDeltatime;
}

TimePoint Timer::Now()
{
    return TimeStamp::now();
}

DeltaTime Timer::GetGlobalTime()
{
    return DeltaTime( chrono::duration_cast<chrono::duration<f32>>(
                        chrono::system_clock::now().time_since_epoch() ).count() );
}

}