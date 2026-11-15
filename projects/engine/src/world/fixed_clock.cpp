#include "bubble/world/fixed_clock.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
FixedClock::FixedClock( u32 stepsPerSecond )
{
    if ( stepsPerSecond == 0 )
        throw logic_error( "FixedClock with 0 steps a second" );
    mDt = 1.0 / stepsPerSecond;
}

u32 FixedClock::Advance( f64 elapsed )
{
    if ( not( elapsed >= 0 ) )
        throw logic_error( format( "FixedClock::Advance( {} ): time goes forward", elapsed ) );
    mAccumulated += min( elapsed, cMostPerFrame );
    u32 due = 0;
    while ( mAccumulated >= mDt )
    {
        mAccumulated -= mDt;
        ++due;
    }
    return due;
}
}
