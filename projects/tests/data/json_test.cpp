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

TEST_CASE( "One key twice is kept twice, and the first of them answers" )
{
    const Json json = Parsed( R"({ "a": 1, "b": 2, "a": 3 })" );
    const auto objectMaybe = json.TryAs<Json::Object>();
    REQUIRE( objectMaybe->size() == 3 );
    // Nothing says a file may not do this, and nothing keeps it from loading:
    // the last one is in the tree but never found.
    CHECK( *json.TryFind( "a" )->TryAs<f64>() == 1.0 );
    CHECK( ( *objectMaybe )[2].mKey == "a" );
    CHECK( Parsed( json.Write() ).Write() == json.Write() );
}

TEST_CASE( "JSON has no comments and no comma before a closing bracket" )
{
    CHECK( Error( "[1,]" ) == "1:4: a value expected, got ]" );
    CHECK( Error( "{ \"a\": 1, }" ) == "1:11: a key in quotes expected, got }" );
    CHECK( Error( "[1 // two]" ) == "1:4: , or ] expected, got /" );
    CHECK( Error( "{ /* a */ \"a\": 1 }" ) == "1:3: a key in quotes expected, got /" );
    CHECK( Error( "[1, 2,]" ) == "1:7: a value expected, got ]" );
}

TEST_CASE( "Space means the four characters JSON calls space" )
{
    CHECK( Parsed( " \t\r\n [ 1 ] \n" ).TryAs<Json::Array>()->size() == 1 );
    // A vertical tab and a non-breaking space look like space to a person and
    // are not space to JSON.
    CHECK( Error( "[\v 1]" ).starts_with( "1:2: a value expected, got" ) );
    CHECK( Error( "[1\xC2\xA0, 2]" ).starts_with( "1:3: , or ] expected, got" ) );
    // Only the line feed moves the line; a lone carriage return is one column.
    CHECK( Error( "[1\r2]" ) == "1:4: , or ] expected, got 2" );
    CHECK( Error( "[\n\r\n x]" ).starts_with( "3:2: a value expected, got" ) );
}

TEST_CASE( "A file the editor saved with a byte-order mark is refused, not read" )
{
    // Windows writes one when it saves a file as "UTF-8 with BOM". Here the
    // mark is not space, so it comes back as the character it is: invisible.
    CHECK( Error( "\xEF\xBB\xBF"
                  "{}" ) == "1:1: a value expected, got \xEF\xBB\xBF" );
}

TEST_CASE( "A number in a file follows JSON's shape, and a double's reach" )
{
    CHECK( *Parsed( "1e2" ).TryAs<f64>() == 100.0 );
    CHECK( *Parsed( "1E+2" ).TryAs<f64>() == 100.0 );
    CHECK( *Parsed( "1e-2" ).TryAs<f64>() == 0.01 );
    // The exponent may lead with zeros, the part before the point may not.
    CHECK( *Parsed( "1e002" ).TryAs<f64>() == 100.0 );
    CHECK( *Parsed( "0e0" ).TryAs<f64>() == 0.0 );
    CHECK( *Parsed( "0.0" ).TryAs<f64>() == 0.0 );
    CHECK( Error( "+1" ) == "1:1: a value expected, got +" );
    CHECK( Error( ".5" ) == "1:1: a value expected, got ." );
    CHECK( Error( "1e" ) == "1:3: a digit expected in the exponent, got the end" );
    CHECK( Error( "1e-" ) == "1:4: a digit expected in the exponent, got the end" );
    CHECK( Error( "1e+e" ) == "1:4: a digit expected in the exponent, got e" );
    // Past either end of a double it is an error, not a huge number or a zero:
    // a file that says 1e-999 does not quietly mean 0.
    CHECK( Error( "1e309" ) == "1:6: 1e309 is past what a double holds" );
    CHECK( Error( "1e-999" ) == "1:7: 1e-999 is past what a double holds" );
    CHECK( Error( string( 400, '9' ) ).find( "is past what a double holds" ) != string::npos );
    // The ends of a double, written and read back whole.
    for ( const f64 number : { 5e-324, 1.7976931348623157e308, 0.1, 1.0 / 3.0, 2.2250738585072014e-308 } )
        CHECK( *Parsed( Json( number ).Write() ).TryAs<f64>() == number );
    // Zero keeps which side of it the sign was on, so a -0 in a file stays -0.
    CHECK( Json( -0.0 ).Write() == "-0\n" );
    CHECK( Json( *Parsed( "-0" ).TryAs<f64>() ).Write() == "-0\n" );
}

TEST_CASE( "What a number JSON cannot write turns into, and describes itself as" )
{
    for ( const f64 broken : { f64( INFINITY ), f64( -INFINITY ), f64( NAN ) } )
    {
        CHECK( Json( broken ).Write() == "null\n" );
        CHECK( Json( broken ).Describe() == "null" );
    }
    CHECK( Parsed( "null" ).IsNull() );
    // A value that was written null and read back is null, not the number it
    // was: the file keeps neither the infinity nor a word saying why.
    CHECK( Parsed( Json( f64( INFINITY ) ).Write() ).IsNull() );
}

TEST_CASE( "Escapes carry the control characters a file cannot hold raw" )
{
    CHECK( *Parsed( R"("\b\f\r")" ).TryAs<string>() == "\b\f\r" );
    // A NUL survives the file and comes back escaped, so the text still reads
    // back as one string of one byte.
    const Json nul = Parsed( R"("\u0000")" );
    REQUIRE( nul.TryAs<string>()->size() == 1 );
    CHECK( nul.Write() == "\"\\u0000\"\n" );
    CHECK( Parsed( nul.Write() ).Write() == nul.Write() );
    // DEL and what is above it are not control characters to JSON.
    CHECK( *Parsed( "\"\x7F\"" ).TryAs<string>() == "\x7F" );
    CHECK( Parsed( "\"\x7F\"" ).Write() == "\"\x7F\"\n" );
    // Keys are strings like any other, escapes and all.
    CHECK( Parsed( R"({ "\u0041": 1 })" ).TryFind( "A" ) );
    CHECK( Parsed( R"({ "a\/b": 1 })" ).TryFind( "a/b" ) );
    // A JSON string is not put through the text's normalization: the two ways
    // of writing "café" stay two different strings until Text reads one.
    CHECK( *Parsed( R"("cafe\u0301")" ).TryAs<string>() != *Parsed( R"("café")" ).TryAs<string>() );
    // A capital U is not an escape; a surrogate must find its pair.
    CHECK( Error( R"("\U0039")" ).starts_with( R"(1:3: \U is not an escape)" ) );
    CHECK( Error( R"("\ud83dA")" ) == R"(1:8: \u escape: a high surrogate with no low one after it)" );
    CHECK( Error( R"("\ud83d\ud83d")" ) == R"(1:14: \u escape: a high surrogate with no low one after it)" );
    // The pair opens but its second escape is not a low surrogate.
    CHECK( Error( R"("\ud800A")" ).ends_with( "a high surrogate with no low one after it" ) );
    CHECK( Error( R"("\ud800\u0041")" ).ends_with( "a high surrogate with no low one after it" ) );
    CHECK( Error( R"("\u")" ) == R"(1:4: \u escape: four hex digits expected)" );
    CHECK( Error( "\"(\\u00)" ).find( "four hex digits expected" ) != string::npos );
    CHECK( Error( "\"\\" ) == "1:3: the end of the text inside a string" );
    // A raw control character says what to write instead, and names the
    // character it is not.
    CHECK( Error( "\"a\tb\"" ).starts_with( R"(1:3: a control character inside a string: write a line break as \n)" ) );
}

TEST_CASE( "Nesting stops one level past what a file should hold" )
{
    const string deep = string( 257, '[' ) + string( 257, ']' );
    CHECK( Parsed( deep ).TryAs<Json::Array>()->size() == 1 );
    const string tooDeep = string( 258, '[' ) + string( 258, ']' );
    CHECK( Error( tooDeep ) == "1:258: nested deeper than 256" );
    // The same depth reached through objects, which is what a broken file
    // really looks like.
    string nested;
    for ( int i = 0; i < 300; ++i )
        nested += "{ \"a\": ";
    nested += "1";
    nested += string( 300, '}' );
    CHECK( Error( nested ).find( "nested deeper than 256" ) != string::npos );
}

TEST_CASE( "A value describes itself the way a message will say it" )
{
    CHECK( Json().Describe() == "null" );
    CHECK( Json( true ).Describe() == "true" );
    CHECK( Json( false ).Describe() == "false" );
    CHECK( Json( 2.5 ).Describe() == "2.5" );
    CHECK( Json( string( "door" ) ).Describe() == "\"door\"" );
    CHECK( Json( string( "a\"b" ) ).Describe() == "\"a\\\"b\"" );
    CHECK( Json( Json::Array{ Json(), Json() } ).Describe() == "an array of 2" );
    CHECK( Json( Json::Array{} ).Describe() == "an array of 0" );
    CHECK( Json( Json::Object{} ).Describe() == "an object" );
    CHECK( Json( Json::Object{ { "a", Json() } } ).Describe() == "an object" );
}

TEST_CASE( "The whole file may be one plain value" )
{
    CHECK( Parsed( "null" ).IsNull() );
    CHECK( *Parsed( "false" ).TryAs<bool>() == false );
    CHECK( *Parsed( "\"door\"" ).TryAs<string>() == "door" );
    CHECK( *Parsed( "42" ).TryAs<f64>() == 42.0 );
    // Nothing to look a key up in: an array or a number answers nothing.
    CHECK_FALSE( Parsed( "[1]" ).TryFind( "a" ) );
    CHECK_FALSE( Parsed( "42" ).TryFind( "a" ) );
    CHECK_FALSE( Parsed( "\"text\"" ).TryFind( "text" ) );
}

TEST_CASE( "Only a plain array rides on one line, and only one member of an object" )
{
    // An array inside an array of scalars is not a scalar, so the outer one
    // goes over lines; the inner one still rides on one.
    CHECK( Parsed( "[ [ 1, 2 ], 3 ]" ).Write() == "[\n  [1, 2],\n  3\n]\n" );
    CHECK( Parsed( R"({ "a": 1 })" ).Write() == "{ \"a\": 1 }\n" );
    CHECK( Parsed( R"({ "a": { "b": 1 } })" ).Write() == "{\n  \"a\": { \"b\": 1 }\n}\n" );
    CHECK( Parsed( R"({ "a": { "b": { "c": 1 } } })" ).Write() == "{\n  \"a\": {\n    \"b\": { \"c\": 1 }\n  }\n}\n" );
    CHECK( Parsed( R"({ "a": [ 1, [ 2 ] ] })" ).Write() == "{\n  \"a\": [\n    1,\n    [2]\n  ]\n}\n" );
    // An empty array and an empty object stay empty either way.
    CHECK( Parsed( "[[],{}]" ).Write() == "[\n  [],\n  {}\n]\n" );
}

TEST_CASE( "A file is read, written, and read again as the same file" )
{
    // The shapes a level file is made of, in one tree, with the odd bits a
    // person can type: escapes, non-ASCII, nested containers, all the kinds.
    const string text = R"({
  "name": "Стул \"один\"\n\t",
  "empty": [],
  "nest": [ [], {}, [ [ 1 ] ] ],
  "flat": { "v": [ 0.0, -0.0, 1e-7, 1e21 ] },
  "deep": { "a": { "b": { "c": [ true, false, null ] } } },
  "odd": { "": "key", "ключ": "\u007F \u00e9 \uD83D\uDE00" }
})";
    const Json json = Parsed( text );
    const string written = json.Write();
    CHECK( Parsed( written ).Write() == written );
    // A second write of the same tree is the same text: a diff shows only what
    // a person changed.
    CHECK( json.Write() == written );
    // -0 keeps its sign through the file.
    const auto flatMaybe = json.TryFind( "flat" )->TryAs<Json::Object>();
    const auto numbersMaybe = ( *flatMaybe )[0].mValue.TryAs<Json::Array>();
    CHECK( ( *numbersMaybe )[1].Write() == "-0\n" );
    CHECK( written.find( "-0" ) != string::npos );
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
