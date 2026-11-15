// FixedClock: frames of any length into steps of one.
#include "bubble/types/utility.hpp"
#include "bubble/world/fixed_clock.hpp"
#include <doctest.h>
#include <limits>

using namespace bubble;

TEST_CASE( "A frame makes as many steps due as its time holds, and keeps the rest for the next" )
{
    FixedClock clock( 60 );
    CHECK( clock.Dt() == doctest::Approx( 1.0 / 60 ) );
    CHECK( clock.Advance( 1.0 / 60 ) == 1 );
    CHECK( clock.Advance( 1.0 / 30 ) == 2 );
    CHECK( clock.Advance( 1.0 / 120 ) == 0 );
    CHECK( clock.Alpha() == doctest::Approx( 0.5 ) );
    CHECK( clock.Advance( 1.0 / 120 ) == 1 );
    CHECK( clock.Alpha() == doctest::Approx( 0.0 ).epsilon( 1e-6 ) );
    CHECK( clock.Advance( 0 ) == 0 );
}

TEST_CASE( "A long frame counts for a quarter of a second at most: the game slows rather than races" )
{
    FixedClock clock( 60 );
    CHECK( clock.Advance( 10 ) == 15 );
    CHECK( clock.Advance( 1.0 / 60 ) == 1 );
}

TEST_CASE( "No steps a second, or time going back, is a bug" )
{
    CHECK_THROWS_AS( FixedClock( 0 ), logic_error );
    FixedClock clock( 60 );
    CHECK_THROWS_AS( clock.Advance( -1 ), logic_error );
    CHECK_THROWS_AS( clock.Advance( std::numeric_limits<f64>::quiet_NaN() ), logic_error );
}
