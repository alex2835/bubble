// Name: an identifier kept as a hash, its text in one table for the process.
#include "bubble/core/name.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A name literal and the same name from text are one number" )
{
    constexpr Name jump = "jump"_n;
    static_assert( jump.Hash() == 0xa73f5c0du );
    const auto fromTextMaybe = Name::TryFrom( "jump" );
    REQUIRE( fromTextMaybe );
    CHECK( *fromTextMaybe == jump );
    CHECK( jump != "run"_n );
    CHECK( Name().Empty() );
    CHECK_FALSE( jump.Empty() );
}

TEST_CASE( "A name knows its text once it was declared" )
{
    REQUIRE( Name::TryFrom( "outer_cut_off" ) );
    CHECK( "outer_cut_off"_n.ToString() == "outer_cut_off" );
    // Never declared: the hash stands in.
    CHECK( "never_declared_anywhere"_n.ToString().starts_with( "#" ) );
    CHECK( Name().ToString().empty() );
}

TEST_CASE( "A name is ASCII snake_case" )
{
    CHECK( Name::TryFrom( "light2_color" ) );
    for ( const string_view bad : { "", "Jump", "2nd", "_hidden", "jump-high", "jump high", "прыжок" } )
        CHECK( Name::TryFrom( bad ).error().find( "is not a name: a name is ASCII snake_case" ) != string::npos );
}

TEST_CASE( "Two texts with one hash are caught when the second is declared" )
{
    // A known pair for 32-bit FNV-1a.
    REQUIRE( "costarring"_n == "liquid"_n );
    REQUIRE( Name::TryFrom( "costarring" ) );
    CHECK( Name::TryFrom( "liquid" ).error() == "'liquid' and 'costarring' have the same hash; rename one of them" );
    // Declaring the same text again is fine.
    CHECK( Name::TryFrom( "costarring" ) );
}
