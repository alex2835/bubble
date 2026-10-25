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

TEST_CASE( "A value says what it is in one word" )
{
    // The word a message and MayHold go by; a Value made empty is a bool that
    // is false, not a "nothing".
    CHECK( Value().Kind() == "bool" );
    CHECK( Value( false ).Kind() == "bool" );
    CHECK( Value( 3 ).Kind() == "number" );
    CHECK( Value( Make( "door" ) ).Kind() == "text" );
    CHECK( Value( vec3( 1, 2, 3 ) ).Kind() == "vec3" );
    CHECK( Value( Color{ 1, 0, 0, 1 } ).Kind() == "color" );
    CHECK( Value( "calm"_n ).Kind() == "name" );
    CHECK( Value( *AssetId::TryParse( "0123456789abcdef0123456789abcdef" ) ).Kind() == "asset" );
    CHECK( Value( EntityPath{ Make( "/props/chair" ) } ).Kind() == "entity" );
    CHECK( Value( Value::List{} ).Kind() == "list" );
    CHECK( Value( Value::Record{} ).Kind() == "record" );
    // The empty ones still say which of the two they are.
    CHECK( Value( Value::List{} ) != Value( Value::Record{} ) );
}

TEST_CASE( "A value equals only the very same value of the very same kind" )
{
    REQUIRE( Name::TryFrom( "door_name" ) );
    // The same letters as text and as a name are two different values, and a
    // number is not a bool: the kind is part of what equal means.
    CHECK( Value( "door_name"_n ) != Value( Make( "door_name" ) ) );
    CHECK( Value( 1 ) != Value( true ) );
    CHECK( Value( 1 ) == Value( 1.0 ) );
    // A record with its fields in another order is another value, so an edit
    // that only reorders them reads as a change.
    const Value ordered = Value::Record{ { "count"_n, 1 }, { "delay"_n, 2 } };
    const Value reordered = Value::Record{ { "delay"_n, 2 }, { "count"_n, 1 } };
    CHECK( ordered != reordered );
    // A number that is not a number is not even equal to itself, so a prop
    // holding NaN is "changed" every time it is compared.
    CHECK( Value( f64( NAN ) ) != Value( f64( NAN ) ) );
}

TEST_CASE( "A value describes what a person cannot type into a field" )
{
    REQUIRE( Name::TryFrom( "calm" ) );
    CHECK( Value( false ).Describe() == "false" );
    CHECK( Value( Color{ 1, 0.5f, 0, 0.25f } ).Describe() == "color( 1, 0.5, 0, 0.25 )" );
    CHECK( Value( "calm"_n ).Describe() == "name calm" );
    CHECK( Value( *AssetId::TryParse( "0123456789abcdef0123456789abcdef" ) ).Describe() ==
           "asset 0123456789abcdef0123456789abcdef" );
    CHECK( Value( Value::Record{ { "count"_n, 1 } } ).Describe() == "a record" );
    CHECK( Value( Value::Record{} ).Describe() == "a record" );
    CHECK( Value( Value::List{} ).Describe() == "a list of 0" );
}

TEST_CASE( "A type says its name the way the article in front of it reads" )
{
    CHECK( Of( ValueKind::Bool ).Describe() == "a bool" );
    CHECK( Of( ValueKind::Number ).Describe() == "a number" );
    CHECK( Of( ValueKind::Vec3 ).Describe() == "a vec3" );
    CHECK( Of( ValueKind::Color ).Describe() == "a color" );
    CHECK( Of( ValueKind::Name ).Describe() == "a name" );
    CHECK( Of( ValueKind::Asset ).Describe() == "an asset" );
    CHECK( Of( ValueKind::Entity ).Describe() == "an entity" );
    CHECK( Of( ValueKind::Any ).Describe() == "a value" );
    CHECK( Of( ValueKind::List ).Describe() == "a list" );
    // Text goes without an article: a person reads "text expected".
    CHECK( Of( ValueKind::Text ).Describe() == "text" );
    // A list of lists: the article stands only at the front.
    CHECK( ValueType::ListOf( ValueType::ListOf( Of( ValueKind::Vec3 ) ) ).Describe() == "a list of list of vec3" );
    // A record with nothing declared says so with empty brackets.
    CHECK( ValueType::RecordOf( {} ).Describe() == "a record ()" );
    CHECK( ValueType::RecordOf( {} ).mFields.empty() );
}

TEST_CASE( "An integer means a whole number a double can hold exactly" )
{
    CHECK( TryCheck( 2, Of( ValueKind::Integer ), "count" ) );
    CHECK( TryCheck( 2.0, Of( ValueKind::Integer ), "count" ) );
    CHECK( TryCheck( -2, Of( ValueKind::Integer ), "count" ) );
    CHECK( TryCheck( -0.0, Of( ValueKind::Integer ), "count" ) );
    // Up to 2^53 a double counts whole; past it, a number that is written
    // without a point is still not a whole number to rely on.
    CHECK( TryCheck( 9007199254740992.0, Of( ValueKind::Integer ), "count" ) );
    CHECK( TryCheck( 9007199254740994.0, Of( ValueKind::Integer ), "count" ).error() ==
           "count: an integer expected, got 9007199254740994" );
    CHECK( TryCheck( 1e300, Of( ValueKind::Integer ), "count" ).error() == "count: an integer expected, got 1e+300" );
    CHECK( TryCheck( f64( NAN ), Of( ValueKind::Integer ), "count" ).error() == "count: an integer expected, got nan" );
    CHECK( TryCheck( f64( INFINITY ), Of( ValueKind::Integer ), "count" ).error() ==
           "count: an integer expected, got inf" );
}

TEST_CASE( "Check walks into a list inside a list and says where it stopped" )
{
    REQUIRE( Name::TryFrom( "count" ) );
    REQUIRE( Name::TryFrom( "inner" ) );
    const ValueType grid = ValueType::ListOf( ValueType::ListOf( Of( ValueKind::Integer ) ) );
    CHECK( TryCheck( Value::List{ Value::List{ 1, 2 }, Value::List{ 3, 2.5 } }, grid, "grid" ).error() ==
           "grid[2][2]: an integer expected, got 2.5" );
    CHECK( TryCheck( Value::List{ 1 }, grid, "grid" ).error() == "grid[1]: a list of integer expected, got 1" );
    // Positions count from 1, as in Luau: the first element is [1].
    CHECK( TryCheck( Value::List{ 2.5 }, ValueType::ListOf( Of( ValueKind::Integer ) ), "grid" ).error() ==
           "grid[1]: an integer expected, got 2.5" );
    // A record inside a record keeps the whole path.
    const ValueType outer = ValueType::RecordOf( {
        { "inner"_n, ValueType::RecordOf( { { "count"_n, Of( ValueKind::Integer ) } } ) },
    } );
    CHECK( TryCheck( Value::Record{ { "inner"_n, Value::Record{ { "count"_n, 1.5 } } } }, outer, "outer" ).error() ==
           "outer.inner.count: an integer expected, got 1.5" );
    CHECK( TryCheck( Value::Record{ { "inner"_n, 1 } }, outer, "outer" ).error() ==
           "outer.inner: a record (count) expected, got 1" );
    // Any accepts everything, and a record with fields nothing is declared for
    // is still checked against nothing.
    CHECK( TryCheck( Value( Make( "x" ) ), Of( ValueKind::Any ), "anything" ) );
    CHECK( TryCheck( Value( EntityPath{ Make( "/a" ) } ), Of( ValueKind::Any ), "anything" ) );
    CHECK( TryCheck( Value::Record{}, ValueType::RecordOf( {} ), "empty" ) );
    CHECK( TryCheck( Value::Record{ { "count"_n, 1 } }, ValueType::RecordOf( {} ), "empty" ).error() ==
           "empty.count: not a field (fields: )" );
}

TEST_CASE( "Check on a list type that does not say of what is a bug in the code" )
{
    // A file reads such a type as a list of anything, but Check cannot walk it
    // and says so by throwing: the declaration came from code that should have
    // named an element type.
    CHECK_THROWS_AS( (void)TryCheck( Value::List{ 1 }, Of( ValueKind::List ), "waves" ), logic_error );
    CHECK_THROWS_WITH( (void)TryCheck( Value::List{ 1 }, Of( ValueKind::List ), "waves" ),
                       "waves: a list type without an element type" );
    // The type is looked at before the elements, so even an empty list of an
    // undeclared element throws.
    CHECK_THROWS_AS( (void)TryCheck( Value::List{}, Of( ValueKind::List ), "waves" ), logic_error );
}

TEST_CASE( "MayHold compares the kind declared, not a kind close to it" )
{
    // An integer is held as a number, yet a field declared an integer is not
    // asked of as a number: the kinds are compared as they are written.
    CHECK_FALSE( MayHold( Of( ValueKind::Integer ), ValueKind::Number ) );
    CHECK_FALSE( MayHold( Of( ValueKind::Number ), ValueKind::Integer ) );
    CHECK( MayHold( Of( ValueKind::Any ), ValueKind::Any ) );
    CHECK( MayHold( Of( ValueKind::List ), ValueKind::List ) );
    // A record with nothing declared holds nothing.
    CHECK_FALSE( MayHold( ValueType::RecordOf( {} ), ValueKind::Any ) );
    CHECK( MayHold( ValueType::RecordOf( {} ), ValueKind::Record ) );
    CHECK( MayHold( ValueType::ListOf( Of( ValueKind::Entity ) ), ValueKind::Entity ) );
    CHECK_FALSE( MayHold( ValueType::ListOf( Of( ValueKind::Entity ) ), ValueKind::Text ) );
    // A list nested in a list is walked into too.
    CHECK( MayHold( ValueType::ListOf( ValueType::ListOf( Of( ValueKind::Entity ) ) ), ValueKind::Entity ) );
}

TEST_CASE( "WidenFloat gives the short decimal a person wrote, not the float's noise" )
{
    // 0.1f as a double straight is 0.10000000149011612; what a file should
    // hold is the 0.1 that reads back as the same float.
    CHECK( WidenFloat( 0.1f ) == 0.1 );
    CHECK( WidenFloat( 1.0f / 3.0f ) != static_cast<f64>( 1.0f / 3.0f ) );
    CHECK( static_cast<f64>( 1.0f / 3.0f ) != 1.0 / 3.0 );
    for ( const f32 value : { 0.0f, -0.0f, 1.0f, -2.5f, 1.0f / 3.0f, 1e-45f, 3.4028235e38f } )
        CHECK( static_cast<f32>( WidenFloat( value ) ) == value );
    // A value no decimal text holds falls back to itself instead of turning
    // into zero: the file writes it as null either way.
    CHECK( WidenFloat( static_cast<f32>( INFINITY ) ) == static_cast<f64>( INFINITY ) );
    CHECK( not isfinite( WidenFloat( static_cast<f32>( NAN ) ) ) );
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

TEST_CASE( "A type says whether it can hold a kind anywhere in it" )
{
    CHECK( MayHold( Of( ValueKind::Entity ), ValueKind::Entity ) );
    CHECK_FALSE( MayHold( Waves(), ValueKind::Entity ) );
    CHECK( MayHold( ValueType::ListOf( ValueType::RecordOf( { { "count"_n, Of( ValueKind::Entity ) } } ) ),
                    ValueKind::Entity ) );
    // A Value field may hold anything.
    CHECK( MayHold( ValueType::RecordOf( { { "count"_n, Of( ValueKind::Any ) } } ), ValueKind::Entity ) );
    // So may a list that does not say of what.
    CHECK( MayHold( Of( ValueKind::List ), ValueKind::Entity ) );
}
