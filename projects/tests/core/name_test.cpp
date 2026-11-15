// Name: an identifier kept as a hash, its text in one table for the process.
#include "bubble/core/name.hpp"
#include "bubble/types/containers.hpp"
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

TEST_CASE( "A name the code declares knows its text, and a bad one is a bug" )
{
    CHECK( Name::Declare( "declared_by_code" ) == "declared_by_code"_n );
    CHECK( "declared_by_code"_n.ToString() == "declared_by_code" );
    CHECK_THROWS_AS( Name::Declare( "Not A Name" ), logic_error );
}

TEST_CASE( "A name is ASCII snake_case" )
{
    CHECK( Name::TryFrom( "light2_color" ) );
    for ( const string_view bad : { "", "Jump", "2nd", "_hidden", "jump-high", "jump high", "прыжок" } )
        CHECK( Name::TryFrom( bad ).error().find( "is not a name: a name is ASCII snake_case" ) != string::npos );
}

TEST_CASE( "A name is letters, digits and underscores, with a letter first" )
{
    // Underscores may sit anywhere but the first place, and any number of
    // them together; digits never lead.
    for ( const string_view good : { "a", "a1", "a_1", "a__b", "a_b_c", "x9y", "a_", "light2_color" } )
        CHECK( Name::TryFrom( good ) );
    for ( const string_view bad : { "_", "_hidden", "a-b", "a b", "a.b", "1", "a\xFF" } )
        CHECK( Name::TryFrom( bad ).error().find( "is not a name: a name is ASCII snake_case" ) != string::npos );
    // A NUL byte in the middle is as much out as one at the end.
    CHECK_FALSE( Name::TryFrom( string_view( "a\0b", 3 ) ) );
    // A trailing underscore is a different name, not a spelling of the shorter one.
    CHECK( "jump"_n != "jump_"_n );
}

TEST_CASE( "A name made from a hash knows its text once someone declares it" )
{
    // "..."_n is hashed at compile time and never tells the table its text;
    // the text of a name comes from From, which files and scripts use.
    constexpr Name pending = "pending_field"_n;
    CHECK( Name::FromHash( pending.Hash() ) == pending );
    CHECK( Name::FromHash( pending.Hash() ).ToString().starts_with( "#" ) );
    REQUIRE( Name::TryFrom( "pending_field" ) );
    CHECK( Name::FromHash( pending.Hash() ).ToString() == "pending_field" );
    CHECK( pending.ToString() == "pending_field" );
    // A hash nobody ever made is printed in full, leading zeros and all.
    CHECK( Name::FromHash( 1 ).ToString() == "#00000001" );
    CHECK( Name().Hash() == 0 );
    CHECK( Name::FromHash( 0 ).Empty() );
}

TEST_CASE( "Names order by their hash, not by their text" )
{
    // Sorting a list of names by the name itself is not sorting it the way a
    // person reads: only the text gives that.
    const Name first = *Name::TryFrom( "aaa_name" );
    const Name second = *Name::TryFrom( "z_name" );
    CHECK( ( first < second ) == ( first.Hash() < second.Hash() ) );
    CHECK( Name() < first );
    set<Name> names{ second, Name(), first };
    CHECK( *names.begin() == Name() );
    CHECK( names.size() == 3 );
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
