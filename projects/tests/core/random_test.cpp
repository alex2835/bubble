// Random: PCG32, the same numbers on every machine for the same seed.
#include "bubble/core/random.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/utility.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "Random gives PCG32's own numbers, and the same seed gives the same ones" )
{
    // The reference generator's demo: seed 42, stream 54.
    Random reference( 42, 54 );
    vector<u32> drawn;
    for ( int i = 0; i < 6; ++i )
        drawn.push_back( reference.Next() );
    CHECK( drawn == vector<u32>{ 0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e } );

    Random a( 7 );
    Random b( 7 );
    Random other( 8 );
    CHECK( a == b );
    CHECK( a.Next() == b.Next() );
    CHECK( a.Next() != other.Next() );

    // A copy goes on from where it was.
    Random copy = a;
    CHECK( copy.Next() == a.Next() );
}

TEST_CASE( "Random's ranges hold what they say" )
{
    Random random( 1 );
    for ( int i = 0; i < 1000; ++i )
    {
        CHECK( random.Below( 6 ) < 6u );
        const f32 unit = random.Float();
        CHECK( ( unit >= 0.0f and unit < 1.0f ) );
        const f32 ranged = random.Range( -2.0f, 3.0f );
        CHECK( ( ranged >= -2.0f and ranged <= 3.0f ) );
    }
    CHECK( random.Below( 1 ) == 0u );
    CHECK_THROWS_AS( random.Below( 0 ), logic_error );
}
