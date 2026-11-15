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
    const AssetCatalog none;
    return TryValueFromJson( *jsonMaybe, type, "light", none );
}

string Written( const Value& value, const ValueType& type )
{
    const AssetCatalog none;
    const auto jsonMaybe = TryValueToJson( value, type, "light", none );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    return jsonMaybe->Write();
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

TEST_CASE( "A tag written where a tag cannot be, and a tag with the wrong thing behind it" )
{
    const ValueType light = LightType();
    // Two members: no tag at all, so the file is read as a record and the
    // person's "$vec3" comes back as a field name that is not a name.
    const auto twoKeysMaybe = TryRead( R"({ "props": { "$vec3": [1, 2, 3], "extra": 1 } })", light );
    REQUIRE_FALSE( twoKeysMaybe );
    CHECK( twoKeysMaybe.error().find( "light.props.$vec3" ) != string::npos );
    CHECK( twoKeysMaybe.error().find( "'$vec3' is not a name" ) != string::npos );
    // The same shape where no tag is allowed: a declared field.
    const auto declaredMaybe = TryRead( R"({ "$vec3": [1, 2, 3] })", light );
    REQUIRE_FALSE( declaredMaybe );
    CHECK( declaredMaybe.error().find( "light.$vec3: '$vec3' is not a name" ) != string::npos );
    // An empty object in an Any field is an empty record, not a broken tag.
    CHECK( TryRead( R"({ "props": {} })", light ) );
    // A tag whose payload is the wrong shape says what the tag wanted - but the
    // tag is not named in the path, so the message says only which field it was
    // reading.
    CHECK( TryRead( R"({ "props": { "$vec3": "up" } })", light )
               .error()
               .ends_with( "light.props: a vec3 expected, got \"up\"" ) );
    CHECK( TryRead( R"({ "props": { "$color": [1, 2, 3, 4, 5] } })", light )
               .error()
               .ends_with( "light.props: a color [r, g, b, a] expected, got an array of 5" ) );
    CHECK( TryRead( R"({ "props": { "$name": "Loud" } })", light ).error().find( "'Loud' is not a name" ) !=
           string::npos );
    CHECK( TryRead( R"({ "props": { "$asset": { "asset": "abc" } } })", light )
               .error()
               .ends_with( "light.props: an asset id - 32 hex digits - expected, got \"abc\"" ) );
    CHECK( TryRead( R"({ "props": { "$entity": 5 } })", light )
               .error()
               .ends_with( "light.props: an entity expected, got 5" ) );
    // A tag inside a list of anything, named by its place.
    CHECK( TryRead( R"({ "props": [ 1, { "$nope": 1 } ] })", light )
               .error()
               .find( "light.props[2]: $nope is not a tag" ) != string::npos );
}

TEST_CASE( "A file's number is held against what the field has to be" )
{
    const ValueType light = LightType();
    // Whole, written with a point: a person writes 3.0 into a count and it is
    // taken for the same whole number.
    const auto wholeMaybe = TryRead( R"({ "count": 3.0 })", light );
    REQUIRE_MESSAGE( wholeMaybe, wholeMaybe.error() );
    const auto countRecordMaybe = wholeMaybe->TryAs<Value::Record>();
    REQUIRE( countRecordMaybe );
    CHECK( *( *countRecordMaybe )[0].mValue.TryAs<f64>() == 3.0 );
    // A number between two whole ones is refused, and so is one too big for a
    // double to count exactly.
    CHECK(
        TryRead( R"({ "count": 1e300 })", light ).error().ends_with( "light.count: an integer expected, got 1e+300" ) );
    CHECK( TryRead( R"({ "count": 9007199254740994 })", light )
               .error()
               .ends_with( "light.count: an integer expected, got 9007199254740994" ) );
    // Quoted, true, or missing: the message says which of them was there.
    CHECK( TryRead( R"({ "count": "3" })", light ).error().ends_with( "light.count: an integer expected, got \"3\"" ) );
    CHECK( TryRead( R"({ "brightness": true })", light )
               .error()
               .ends_with( "light.brightness: a number expected, got true" ) );
    CHECK( TryRead( R"({ "brightness": null })", light )
               .error()
               .ends_with( "light.brightness: a number expected, got null" ) );
    CHECK( TryRead( R"({ "steps": [ 1, [ 2 ] ] })", light )
               .error()
               .ends_with( "light.steps[2]: a number expected, got an array of 1" ) );
    CHECK( TryRead( R"({ "steps": {} })", light )
               .error()
               .ends_with( "light.steps: a list of number expected, got an object" ) );
}

TEST_CASE( "What a file cannot keep exactly, and what it quietly makes whole" )
{
    const ValueType light = LightType();
    // An asset id is 32 hex digits: capitals are not hex here, and a person
    // writing a GUID in the usual form gets it refused.
    CHECK( TryRead( R"({ "mesh": { "asset": "0123456789ABCDEF0123456789ABCDEF" } })", light )
               .error()
               .ends_with( "an asset id - 32 hex digits - expected, got \"0123456789ABCDEF0123456789ABCDEF\"" ) );
    // All zeros parses, and is the same id as "no asset": a file can hold an
    // id that reads as nothing at all.
    const auto zeroMaybe = TryRead( R"({ "mesh": { "asset": "00000000000000000000000000000000" } })", light );
    REQUIRE_MESSAGE( zeroMaybe, zeroMaybe.error() );
    const auto zeroRecordMaybe = zeroMaybe->TryAs<Value::Record>();
    REQUIRE( zeroRecordMaybe );
    const auto idMaybe = ( *zeroRecordMaybe )[0].mValue.TryAs<AssetId>();
    REQUIRE( idMaybe );
    CHECK( not *idMaybe );
    CHECK( *idMaybe == AssetId{} );
    // A color may come with three numbers and goes back with four.
    const auto colorMaybe = TryRead( R"({ "tint": [1, 0, 0] })", light );
    REQUIRE_MESSAGE( colorMaybe, colorMaybe.error() );
    CHECK( Written( *colorMaybe, light ).find( "\"tint\": [1, 0, 0, 1]" ) != string::npos );
    // A color number too small for a float comes out as the nearest float, a
    // zero, the way 0.1 comes out as the float nearest to it.
    const auto smallMaybe = TryRead( R"({ "tint": [ -1e-300, 1e38, 0 ] })", light );
    REQUIRE_MESSAGE( smallMaybe, smallMaybe.error() );
    CHECK( Written( *smallMaybe, light ).find( "\"tint\": [-0, 1e+38, 0, 1]" ) != string::npos );
}

TEST_CASE( "A number past what a float holds is refused as it is read" )
{
    // In the float it would be infinity, which a file cannot hold: the value
    // would never save again.
    const ValueType light = LightType();
    CHECK( TryRead( R"({ "tint": [ 1e300, 0, 0 ] })", light ).error() ==
           "1:13: light.tint[1]: 1e+300 is past what a float holds" );
    CHECK( TryRead( R"({ "color": [ 0, -1e39, 0 ] })", light ).error() ==
           "1:17: light.color[2]: -1e+39 is past what a float holds" );
    CHECK( TryRead( R"({ "props": { "$vec3": [ 0, 0, 4e38 ] } })", light )
               .error()
               .ends_with( "light.props[3]: 4e+38 is past what a float holds" ) );
    // The biggest float, written the way the writer writes it, reads back.
    const auto biggestMaybe = TryRead( R"({ "tint": [ 3.4028235e+38, 0, 0 ] })", light );
    REQUIRE_MESSAGE( biggestMaybe, biggestMaybe.error() );
    CHECK( Written( *biggestMaybe, light ).find( "\"tint\": [3.4028235e+38, 0, 0, 1]" ) != string::npos );
}

TEST_CASE( "A number a file cannot hold is refused as it is written, saying where" )
{
    const ValueType light = LightType();
    const Value nan = Value::Record{ { Declared( "brightness" ), f64( NAN ) } };
    CHECK( TryValueToJson( nan, light, "light", AssetCatalog() ).error() ==
           "light.brightness: nan is a number a file cannot hold" );
    const Value steps = Value::Record{ { Declared( "steps" ), Value::List{ 1, f64( -INFINITY ) } } };
    CHECK( TryValueToJson( steps, light, "light", AssetCatalog() ).error() ==
           "light.steps[2]: -inf is a number a file cannot hold" );
    const Value tint = Value::Record{ { Declared( "tint" ), Color{ 1, static_cast<f32>( INFINITY ), 0, 1 } } };
    CHECK( TryValueToJson( tint, light, "light", AssetCatalog() ).error() ==
           "light.tint: inf is a number a file cannot hold" );
    const Value props = Value::Record{ { Declared( "props" ), Value::Record{ { Declared( "speed" ), f64( NAN ) } } } };
    CHECK( TryValueToJson( props, light, "light", AssetCatalog() ).error() ==
           "light.props.speed: nan is a number a file cannot hold" );
}

TEST_CASE( "An empty record and an empty list keep what they are through a file" )
{
    const ValueType light = LightType();
    const auto emptyMaybe = TryRead( R"({ "props": { "nothing": {}, "list": [], "deep": [ [], [{}] ] } })", light );
    REQUIRE_MESSAGE( emptyMaybe, emptyMaybe.error() );
    const string written = Written( *emptyMaybe, light );
    const auto backMaybe = TryRead( written, light );
    REQUIRE_MESSAGE( backMaybe, backMaybe.error() );
    CHECK( *backMaybe == *emptyMaybe );
    CHECK( Written( *backMaybe, light ) == written );
}

TEST_CASE( "An entity field is the text a person wrote, and nothing checks it" )
{
    const ValueType light = LightType();
    // The file keeps what to look for; whether it finds anything is the
    // scene's question, asked later, where the answer has a path to answer it.
    CHECK( TryRead( R"({ "target": "not a path" })", light ) );
    CHECK( TryRead( R"({ "target": "" })", light ) );
    CHECK( TryRead( R"({ "target": "/../x" })", light ) );
    // Tagged as Any, the same: the value is kept as it was written.
    const auto taggedMaybe = TryRead( R"({ "props": { "goal": { "$entity": "%" } } })", light );
    REQUIRE_MESSAGE( taggedMaybe, taggedMaybe.error() );
    CHECK( Written( *taggedMaybe, light ).find( "\"goal\": { \"$entity\": \"%\" }" ) != string::npos );
    // Text keeps what a keyboard does not type, and comes back the same.
    const auto textMaybe = TryRead( R"({ "props": { "note": "a\tb\u0000c" } })", light );
    REQUIRE_MESSAGE( textMaybe, textMaybe.error() );
    const string textFile = Written( *textMaybe, light );
    CHECK( textFile.find( R"("note": "a\tb\u0000c")" ) != string::npos );
    const auto textBackMaybe = TryRead( textFile, light );
    REQUIRE_MESSAGE( textBackMaybe, textBackMaybe.error() );
    CHECK( *textBackMaybe == *textMaybe );
}

TEST_CASE( "A Value is written plain where its type is known, tagged where it is Any" )
{
    const string text = Written( Light(), LightType() );
    CHECK( text == R"({
  "color": [1, 0.5, 0],
  "brightness": 0.1,
  "count": 3,
  "steps": [1, 2],
  "target": "/props/door",
  "tint": [1, 0, 0, 0.5],
  "mood": "calm",
  "mesh": { "asset": "0123456789abcdef0123456789abcdef" },
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
           R"(1:11: light.mesh: an asset - { "asset": "<id>", "path": "<path>" } - expected, got "abc")" );
    CHECK( TryRead( R"({ "mesh": { "asset": "abc" } })", light ).error() ==
           "1:22: light.mesh: an asset id - 32 hex digits - expected, got \"abc\"" );
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

TEST_CASE( "An asset in a file is its id and its path, found by the path when the id is not" )
{
    AssetCatalog assets;
    const AssetId hero = AssetId::Generate();
    assets.Add( hero, *AssetPath::TryFrom( "models/hero.glb" ) );
    const auto read = [&]( const string& text ) { return TryAssetFromJson( *Json::TryParse( text ), "mesh", assets ); };

    const string written = AssetToJson( hero, assets ).Write();
    CHECK( written.find( R"("path": "models/hero.glb")" ) != string::npos );
    CHECK( read( written ) == hero );
    // The id not found - its .import lost and made again: the asset at the
    // path now.
    const AssetId lost = AssetId::Generate();
    CHECK( read( R"({ "asset": ")" + lost.ToString() + R"(", "path": "models/hero.glb" })" ) == hero );
    // Neither found: kept as written, a link broken until the asset is back.
    CHECK( read( R"({ "asset": ")" + lost.ToString() + R"(", "path": "models/gone.glb" })" ) == lost );
    CHECK( read( R"({ "asset": ")" + lost.ToString() + R"(" })" ) == lost );
    // One the catalog does not have is written without a path.
    CHECK( AssetToJson( lost, assets ).Write().find( "path" ) == string::npos );

    CHECK( read( R"({ "path": "models/hero.glb" })" ).error() ==
           R"(1:1: mesh: an asset without its id: "asset": "<32 hex digits>")" );
    CHECK( read( R"({ "asset": ")" + hero.ToString() + R"(", "guid": 1 })" )
               .error()
               .ends_with( "mesh: guid is not a key of an asset (keys: asset, path)" ) );
    CHECK( read( R"({ "asset": ")" + lost.ToString() + R"(", "path": 3 })" )
               .error()
               .ends_with( "mesh: an asset's path - text - expected, got 3" ) );
}
