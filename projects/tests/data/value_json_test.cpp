// A Value in a file: plain where its type is declared, tagged where it is
// Any, and an error that says where in the text and where in the value.
#include "bubble/data/value_json.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

ValueType Kind( ValueKind kind )
{
    return ValueType::Of( kind );
}

Name Declared( string_view name )
{
    const auto declaredMaybe = Name::TryFrom( name );
    REQUIRE( declaredMaybe );
    return *declaredMaybe;
}

expected<Value, string> TryRead( string_view text, const ValueType& type )
{
    const auto jsonMaybe = Json::TryParse( text );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    return TryValueFromJson( *jsonMaybe, type, "light" );
}

// light: { color, brightness, count, steps, target, tint, mood, mesh, props }
ValueType LightType()
{
    return ValueType::RecordOf( {
        { Declared( "color" ), Kind( ValueKind::Vec3 ) },
        { Declared( "brightness" ), Kind( ValueKind::Number ) },
        { Declared( "count" ), Kind( ValueKind::Integer ) },
        { Declared( "steps" ), ValueType::ListOf( Kind( ValueKind::Number ) ) },
        { Declared( "target" ), Kind( ValueKind::Entity ) },
        { Declared( "tint" ), Kind( ValueKind::Color ) },
        { Declared( "mood" ), Kind( ValueKind::Name ) },
        { Declared( "mesh" ), Kind( ValueKind::Asset ) },
        { Declared( "props" ), Kind( ValueKind::Any ) },
    } );
}

Value Light()
{
    return Value::Record{
        { Declared( "color" ), vec3( 1, 0.5f, 0 ) },
        { Declared( "brightness" ), 0.1 },
        { Declared( "count" ), 3 },
        { Declared( "steps" ), Value::List{ 1, 2 } },
        { Declared( "target" ), EntityPath{ Make( "/props/door" ) } },
        { Declared( "tint" ), Color{ 1, 0, 0, 0.5f } },
        { Declared( "mood" ), Declared( "calm" ) },
        { Declared( "mesh" ), *AssetId::TryParse( "0123456789abcdef0123456789abcdef" ) },
        { Declared( "props" ),
          Value::Record{
              { Declared( "speed" ), 5 },
              { Declared( "spawn" ), vec3( 1, 2, 3 ) },
              { Declared( "goal" ), EntityPath{ Make( "../exit" ) } },
              { Declared( "tags" ), Value::List{ Declared( "red" ), Make( "Стул" ) } },
          } },
    };
}
}

TEST_CASE( "A Value is written plain where its type is known, tagged where it is Any" )
{
    const string text = ValueToJson( Light(), LightType() ).Write();
    CHECK( text == R"({
  "color": [1, 0.5, 0],
  "brightness": 0.1,
  "count": 3,
  "steps": [1, 2],
  "target": "/props/door",
  "tint": [1, 0, 0, 0.5],
  "mood": "calm",
  "mesh": "0123456789abcdef0123456789abcdef",
  "props": {
    "speed": 5,
    "spawn": { "$vec3": [1, 2, 3] },
    "goal": { "$entity": "../exit" },
    "tags": [
      { "$name": "red" },
      "Стул"
    ]
  }
}
)" );

    const auto backMaybe = TryRead( text, LightType() );
    REQUIRE_MESSAGE( backMaybe, backMaybe.error() );
    CHECK( *backMaybe == Light() );
}

TEST_CASE( "A color may leave out its alpha, and a record its fields" )
{
    const auto valueMaybe = TryRead( R"({ "tint": [1, 0, 0] })", LightType() );
    REQUIRE_MESSAGE( valueMaybe, valueMaybe.error() );
    const auto recordMaybe = valueMaybe->TryAs<Value::Record>();
    REQUIRE( recordMaybe->size() == 1 );
    CHECK( ( *recordMaybe )[0].mValue == Value( Color{ 1, 0, 0, 1 } ) );
}

TEST_CASE( "A Value that does not fit its type says where in the text and in the value" )
{
    const ValueType light = LightType();
    CHECK( TryRead( R"({ "brightness": "bright" })", light ).error() ==
           "1:17: light.brightness: a number expected, got \"bright\"" );
    CHECK( TryRead( R"({ "count": 2.5 })", light ).error() == "1:12: light.count: an integer expected, got 2.5" );
    CHECK( TryRead( R"({ "color": [1, 2] })", light ).error() ==
           "1:12: light.color: a vec3 expected, got an array of 2" );
    CHECK( TryRead( R"({ "tint": "red" })", light ).error() ==
           "1:11: light.tint: a color [r, g, b, a] expected, got \"red\"" );
    CHECK( TryRead( R"({ "steps": [1, true] })", light ).error() ==
           "1:16: light.steps[2]: a number expected, got true" );
    CHECK( TryRead( R"({ "mesh": "abc" })", light ).error() ==
           "1:11: light.mesh: an asset id - 32 hex digits - expected, got \"abc\"" );
    CHECK( TryRead( R"({ "mood": "Calm" })", light ).error().starts_with( "1:11: light.mood: 'Calm' is not a name" ) );
    CHECK(
        TryRead( R"({ "colour": [1, 1, 1] })", light ).error() ==
        "1:13: light.colour: not a field (fields: color, brightness, count, steps, target, tint, mood, mesh, props)" );
    CHECK( TryRead( R"({ "count": 1, "count": 2 })", light ).error() == "1:24: light.count: given twice" );
    CHECK( TryRead( R"([1])", light ).error() == "1:1: light: a record (color, brightness, count, steps, target, tint, "
                                                 "mood, mesh, props) expected, got an array of 1" );

    CHECK( TryRead( R"({ "props": { "at": { "$vec4": [1, 2, 3, 4] } } })", light ).error() ==
           "1:20: light.props.at: $vec4 is not a tag (tags: $vec3, $color, $name, $asset, $entity)" );
    CHECK( TryRead( R"({ "props": { "at": null } })", light ).error() ==
           "1:20: light.props.at: a value expected, got null" );
}
