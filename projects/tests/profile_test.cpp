#include "bubble/core/profile.hpp"
#include "bubble/types/number.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
i64 Fibonacci( i64 n )
{
    BUBBLE_PROFILE_ZONE();
    return n < 2 ? n : Fibonacci( n - 1 ) + Fibonacci( n - 2 );
}
}

// The macros compile and leave the code's meaning alone with the profiler
// on and off; with it on and no profiler connected nothing is recorded.
TEST_CASE( "Profiler zones do not change what the code does" )
{
    BUBBLE_PROFILE_THREAD_NAME( "tests" );
    i64 result = 0;
    {
        BUBBLE_PROFILE_ZONE_NAMED( "fibonacci" );
        result = Fibonacci( 15 );
    }
    BUBBLE_PROFILE_PLOT( "fibonacci", result );
    BUBBLE_PROFILE_FRAME();
    CHECK( result == 610 );
}
