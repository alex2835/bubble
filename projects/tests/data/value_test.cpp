// Value: what a file keeps for a prop or a parameter, and ValueType, what a
// declaration says it has to be.
#include "bubble/data/value.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

ValueType Of( ValueKind kind )
{
    return ValueType{ kind, nullptr, {} };
}

// waves: a list of { count: integer, delay: number }.
ValueType Waves()
{
    REQUIRE( Name::TryFrom( "count" ) );
    REQUIRE( Name::TryFrom( "delay" ) );
    return ValueType::ListOf(
        ValueType::RecordOf( { { "count"_n, Of( ValueKind::Integer ) }, { "delay"_n, Of( ValueKind::Number ) } } ) );
}

Value Wave( Value count, Value delay )
{
    return Value::Record{ { "count"_n, std::move( count ) }, { "delay"_n, std::move( delay ) } };
}
}

TEST_CASE( "A value holds one of a closed set of kinds" )
{
    CHECK( Value().Is<bool>() );
    CHECK( Value( 3 ).Is<f64>() );
    CHECK( *Value( 3 ).TryAs<f64>() == 3.0 );
    CHECK( Value( Make( "door" ) ).TryAs<Text>()->View() == "door" );
    CHECK( Value( vec3( 1, 2, 3 ) ).Is<vec3>() );
    CHECK_FALSE( Value( 2.5 ).TryAs<Text>() );
    CHECK( Value( 2.5 ) == Value( 2.5 ) );
    CHECK( Value( 2.5 ) != Value( true ) );
    CHECK( Value( Value::List{ 1, 2 } ) == Value( Value::List{ 1, 2 } ) );
}

TEST_CASE( "A value describes itself for messages" )
{
    CHECK( Value( true ).Describe() == "true" );
    CHECK( Value( 2.5 ).Describe() == "2.5" );
    CHECK( Value( Make( "door" ) ).Describe() == "\"door\"" );
    CHECK( Value( vec3( 1, 2, 3 ) ).Describe() == "vec3( 1, 2, 3 )" );
    CHECK( Value( Value::List{ 1, 2, 3 } ).Describe() == "a list of 3" );
    CHECK( Value( EntityPath{ Make( "/props/chair" ) } ).Describe() == "entity /props/chair" );
}

TEST_CASE( "A type describes itself for messages" )
{
    CHECK( Of( ValueKind::Integer ).Describe() == "an integer" );
    CHECK( Of( ValueKind::Text ).Describe() == "text" );
    CHECK( ValueType::ListOf( Of( ValueKind::Vec3 ) ).Describe() == "a list of vec3" );
    CHECK( Waves().Describe() == "a list of record (count, delay)" );
}

TEST_CASE( "Check passes a value that is what its type says" )
{
    CHECK( TryCheck( 2, Of( ValueKind::Integer ), "count" ) );
    CHECK( TryCheck( 2.5, Of( ValueKind::Number ), "speed" ) );
    CHECK( TryCheck( Value::List{ Wave( 3, 1.5 ), Wave( 5, 0.5 ) }, Waves(), "waves" ) );
    CHECK( TryCheck( Value::List{}, Waves(), "waves" ) );
}

TEST_CASE( "Check says where a value goes wrong and what it got" )
{
    CHECK( TryCheck( 2.5, Of( ValueKind::Integer ), "count" ).error() == "count: an integer expected, got 2.5" );
    CHECK( TryCheck( Make( "fast" ), Of( ValueKind::Number ), "speed" ).error() ==
           "speed: a number expected, got \"fast\"" );
    CHECK( TryCheck( Value::List{ Wave( 3, 1.5 ), Wave( 2.5, 1 ) }, Waves(), "waves" ).error() ==
           "waves[2].count: an integer expected, got 2.5" );
    CHECK( TryCheck( Value::List{ Value::Record{ { "count"_n, 1 } } }, Waves(), "waves" ).error() ==
           "waves[1]: no field delay" );
    REQUIRE( Name::TryFrom( "cnt" ) );
    CHECK( TryCheck( Value::List{ Value::Record{ { "cnt"_n, 1 }, { "delay"_n, 1 } } }, Waves(), "waves" ).error() ==
           "waves[1].cnt: not a field (fields: count, delay)" );
    CHECK( TryCheck( Value::List{ Value::Record{ { "count"_n, 1 }, { "count"_n, 2 }, { "delay"_n, 1 } } }, Waves(),
                     "waves" )
               .error() == "waves[1].count: given twice" );
    CHECK( TryCheck( 1, Waves(), "waves" ).error() == "waves: a list of record (count, delay) expected, got 1" );
}
