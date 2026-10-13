// JSON: parsed into a tree that knows where each value started, written
// back the same way every time, and an error that says line and column.
#include "bubble/data/json.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Json Parsed( string_view text )
{
    auto jsonMaybe = Json::TryParse( text );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    return *jsonMaybe;
}

string Error( string_view text )
{
    const auto jsonMaybe = Json::TryParse( text );
    REQUIRE_FALSE( jsonMaybe );
    return jsonMaybe.error();
}
}

TEST_CASE( "JSON is parsed into a tree, members in their order" )
{
    const Json json = Parsed( R"({ "b": [1, -2.5, 1e3, 0], "a": true, "n": null, "s": "door" })" );
    const auto objectMaybe = json.TryAs<Json::Object>();
    REQUIRE( objectMaybe );
    REQUIRE( objectMaybe->size() == 4 );
    CHECK( ( *objectMaybe )[0].mKey == "b" );
    CHECK( ( *objectMaybe )[1].mKey == "a" );

    const auto numbersMaybe = json.TryFind( "b" )->TryAs<Json::Array>();
    REQUIRE( numbersMaybe );
    CHECK( *( *numbersMaybe )[1].TryAs<f64>() == -2.5 );
    CHECK( *( *numbersMaybe )[2].TryAs<f64>() == 1000.0 );
    CHECK( *json.TryFind( "a" )->TryAs<bool>() == true );
    CHECK( json.TryFind( "n" )->IsNull() );
    CHECK( *json.TryFind( "s" )->TryAs<string>() == "door" );
    CHECK_FALSE( json.TryFind( "missing" ) );
}

TEST_CASE( "A string's escapes are read, \\u pairs included" )
{
    CHECK( *Parsed( R"("a\"b\\c\/d\n\t")" ).TryAs<string>() == "a\"b\\c/d\n\t" );
    CHECK( *Parsed( R"("café")" ).TryAs<string>() == "caf\xC3\xA9" );
    // U+1F600, a pair of surrogates.
    CHECK( *Parsed( R"("😀")" ).TryAs<string>() == "\xF0\x9F\x98\x80" );
    CHECK( *Parsed( "\"Стул\"" ).TryAs<string>() == "Стул" );
}

TEST_CASE( "Every value knows where it started" )
{
    const Json json = Parsed( "{\n  \"a\": [1,\n    \"ё\", 3]\n}" );
    CHECK( json.Where() == "1:1" );
    const auto arrayMaybe = json.TryFind( "a" )->TryAs<Json::Array>();
    CHECK( json.TryFind( "a" )->Where() == "2:8" );
    CHECK( ( *arrayMaybe )[1].Where() == "3:5" );
    // Columns count code points: "ё" is two bytes, one column.
    CHECK( ( *arrayMaybe )[2].Where() == "3:10" );
    CHECK( Json( 1.0 ).Where().empty() );
}

TEST_CASE( "A broken text says where and what is there instead" )
{
    CHECK( Error( "{\n  \"a\": 1,\n  \"b\" 2\n}" ) == "3:7: : expected after the key, got 2" );
    CHECK( Error( "[1, 2" ) == "1:6: , or ] expected, got the end" );
    CHECK( Error( "{\"a\": 1 \"b\": 2}" ) == "1:9: , or } expected, got \"" );
    CHECK( Error( "{a: 1}" ) == "1:2: a key in quotes expected, got a" );
    CHECK( Error( "[\"ё\", x]" ) == "1:7: a value expected, got x" );
    CHECK( Error( "nul" ) == "1:1: a value expected, got n" );
    CHECK( Error( "" ) == "1:1: a value expected, got the end" );
    CHECK( Error( "{} x" ) == "1:4: the end expected after the value, got x" );
    CHECK( Error( "[01]" ) == "1:3: a number does not start with 0 unless it is 0" );
    CHECK( Error( "[1.]" ) == "1:4: a digit expected after the point, got ]" );
    CHECK( Error( "[-]" ) == "1:3: a digit expected, got ]" );
    CHECK( Error( "1e999" ) == "1:6: 1e999 is past what a double holds" );
    CHECK( Error( "\"abc" ) == "1:5: the end of the text inside a string" );
    CHECK( Error( "\"a\nb\"" ).starts_with( "1:3: a control character inside a string" ) );
    CHECK( Error( R"("a\qb")" ).starts_with( "1:4: \\q is not an escape" ) );
    CHECK( Error( R"("\ud83d")" ) == "1:8: \\u escape: a high surrogate with no low one after it" );
    CHECK( Error( R"("\ude00")" ) == "1:8: \\u escape: a low surrogate with no high one before it" );
    CHECK( Error( R"("\u12")" ) == "1:6: \\u escape: four hex digits expected" );
    CHECK( Error( "\"\xFF\"" ) == "the text is not valid UTF-8" );
    CHECK( Error( string( 300, '[' ) ) == "1:258: nested deeper than 256" );
}

TEST_CASE( "JSON is written indented, short arrays on one line" )
{
    Json::Object inner;
    inner.push_back( Json::Member{ "$vec3", Json( Json::Array{ Json( 1.0 ), Json( 2.0 ), Json( 3.0 ) } ) } );
    Json::Object object;
    object.push_back( Json::Member{ "name", Json( string( "lamp \"one\"\n" ) ) } );
    object.push_back( Json::Member{ "color", Json( Json::Array{ Json( 1.0 ), Json( 0.5 ), Json( 0.0 ) } ) } );
    object.push_back( Json::Member{ "at", Json( std::move( inner ) ) } );
    object.push_back( Json::Member{ "empty", Json( Json::Object{} ) } );
    object.push_back( Json::Member{ "list", Json( Json::Array{ Json( Json::Object{} ), Json() } ) } );
    object.push_back( Json::Member{ "on", Json( true ) } );

    const string text = Json( std::move( object ) ).Write();
    CHECK( text == R"({
  "name": "lamp \"one\"\n",
  "color": [1, 0.5, 0],
  "at": { "$vec3": [1, 2, 3] },
  "empty": {},
  "list": [
    {},
    null
  ],
  "on": true
}
)" );
    // Read back and written again: the same text.
    CHECK( Parsed( text ).Write() == text );
}

TEST_CASE( "A number is written as the shortest text that reads back the same" )
{
    CHECK( Json( 0.1 ).Write() == "0.1\n" );
    CHECK( Json( 3.0 ).Write() == "3\n" );
    CHECK( Json( -0.25 ).Write() == "-0.25\n" );
    CHECK( Json( 1e21 ).Write() == "1e+21\n" );
    CHECK( *Parsed( Json( 0.1 + 0.2 ).Write() ).TryAs<f64>() == 0.1 + 0.2 );
    // JSON has no word for these.
    CHECK( Json( f64( INFINITY ) ).Write() == "null\n" );
}
