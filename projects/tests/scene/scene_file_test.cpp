// A scene as a file: saved as nested JSON, loaded back into the same scene,
// and a file that is wrong says where.
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    Color mColor;
    f32 mBrightness = 1;
    i32 mFlickers = 0;
    vector<f32> mSteps;
    EntityPath mTarget;
    Value mProps = Value::Record{};
    // Rebuilt, never saved.
    f32 mCache = 0;
};

struct Health
{
    i32 mValue = 100;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

Name Declared( string_view name )
{
    const auto declaredMaybe = Name::TryFrom( name );
    REQUIRE( declaredMaybe );
    return *declaredMaybe;
}

struct Fixture
{
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };

    Fixture()
    {
        mReflection.Type<Light>( "light" )
            .Field<&Light::mColor>( "color" )
            .Field<&Light::mBrightness>( "brightness" )
            .Field<&Light::mFlickers>( "flickers" )
            .Field<&Light::mSteps>( "steps" )
            .Field<&Light::mTarget>( "target" )
            .Field<&Light::mProps>( "props" )
            .Field<&Light::mCache>( "cache", { .mFlags = FieldInfo::Transient } );
        mReflection.Type<Health>( "health" ).Field<&Health::mValue>( "value" );
        mComponentTypes.Add<Light>();
        mComponentTypes.Add<Health>();
    }

    // /lamp with a light, /props/chair with health, and an entity outside the tree.
    Scene Made() const
    {
        Scene scene;
        const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
        Light& light = scene.Add<Light>( lamp );
        light.mColor = Color{ 1, 0.5f, 0, 1 };
        light.mBrightness = 0.1f;
        light.mFlickers = 2;
        light.mSteps = { 1, 2 };
        light.mTarget = EntityPath{ Make( "/props/chair" ) };
        light.mProps = Value::Record{ { Declared( "speed" ), 5 }, { Declared( "spawn" ), vec3( 1, 2, 3 ) } };
        light.mCache = 7;

        const Entity props = CreateChild( scene, scene.Root(), Make( "props" ) );
        scene.Add<Health>( CreateChild( scene, props, Make( "chair" ) ) ).mValue = 50;
        scene.Add<Health>( scene.Create() );
        return scene;
    }

    string Save( const Scene& scene ) const
    {
        const auto textMaybe = TrySaveScene( scene, mComponentTypes );
        REQUIRE_MESSAGE( textMaybe, textMaybe.error() );
        return *textMaybe;
    }

    Scene Load( string_view text ) const
    {
        auto sceneMaybe = TryLoadScene( text, mComponentTypes );
        REQUIRE_MESSAGE( sceneMaybe, sceneMaybe.error() );
        return std::move( *sceneMaybe );
    }

    string Why( string_view text ) const
    {
        const auto sceneMaybe = TryLoadScene( text, mComponentTypes );
        REQUIRE_FALSE( sceneMaybe );
        return sceneMaybe.error();
    }
};

constexpr string_view cSaved = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "lamp",
        "components": {
          "light": {
            "color": [1, 0.5, 0, 1],
            "brightness": 0.1,
            "flickers": 2,
            "steps": [1, 2],
            "target": "/props/chair",
            "props": {
              "speed": 5,
              "spawn": { "$vec3": [1, 2, 3] }
            }
          }
        }
      },
      {
        "name": "props",
        "children": [
          {
            "name": "chair",
            "components": {
              "health": { "value": 50 }
            }
          }
        ]
      }
    ]
  }
}
)";
}

TEST_CASE( "A scene is saved as its tree, nested, transient fields and loose entities left out" )
{
    Fixture f;
    CHECK( f.Save( f.Made() ) == cSaved );
    CHECK( f.Save( Scene() ) == "{\n  \"version\": 1,\n  \"root\": {}\n}\n" );
}

TEST_CASE( "A scene loaded from its file is the same scene, and saves to the same file" )
{
    Fixture f;
    const Scene scene = f.Load( cSaved );
    CHECK( f.Save( scene ) == cSaved );

    const Entity lamp = *TryResolve( scene, EntityPath{ Make( "/lamp" ) } );
    const auto lightMaybe = scene.TryGet<Light>( lamp );
    REQUIRE( lightMaybe );
    CHECK( lightMaybe->mBrightness == 0.1f );
    CHECK( lightMaybe->mColor.mG == 0.5f );
    CHECK( lightMaybe->mSteps == vector<f32>{ 1, 2 } );
    CHECK( lightMaybe->mTarget == EntityPath{ Make( "/props/chair" ) } );
    CHECK( lightMaybe->mCache == 0.0f );
    // What the path names is there.
    CHECK( TryResolve( scene, lightMaybe->mTarget ) );
    CHECK( scene.Get<Health>( *TryResolve( scene, EntityPath{ Make( "/props/chair" ) } ) ).mValue == 50 );
}

TEST_CASE( "A field a file leaves out keeps its default" )
{
    Fixture f;
    const Scene scene =
        f.Load( R"({ "version": 1, "root": { "children": [ { "name": "lamp", "components": { "light": {} } } ] } })" );
    const auto lightMaybe = scene.TryGet<Light>( *TryResolve( scene, EntityPath{ Make( "/lamp" ) } ) );
    REQUIRE( lightMaybe );
    CHECK( lightMaybe->mBrightness == 1.0f );
}

TEST_CASE( "A wrong scene file says where and what is wrong" )
{
    Fixture f;
    CHECK( f.Why( "{ \"version\": 1, \"root\": {\n  \"children\": [ { \"name\": \"lamp\", \"components\": {\n    "
                  "\"light\": { \"brightness\": \"bright\" } } } ] } }" ) ==
           "3:30: /lamp: light.brightness: a number expected, got \"bright\"" );
    CHECK( f.Why( R"({ "version": 1, "root": { "components": { "lihgt": {} } } })" ) ==
           "1:52: /: lihgt is not a component (components: light, health)" );
    CHECK( f.Why( R"({ "version": 1, "root": { "children": [ { "name": "a" }, { "name": "a" } ] } })" ) ==
           "1:68: /: a second child named \"a\"" );
    CHECK( f.Why( R"({ "version": 1, "root": { "children": [ { "name": "a/b" } ] } })" ) ==
           "1:51: /: \"a/b\" is not a name an entity can have: not empty, no /, not . or .." );
    CHECK( f.Why( R"({ "version": 1, "root": { "children": [ { "components": {} } ] } })" ) ==
           "1:41: /: a child with no name" );
    CHECK( f.Why( R"({ "version": 1, "root": { "name": "world" } })" ) == "1:35: /: the root has no name" );
    CHECK( f.Why( R"({ "version": 1, "root": { "child": [] } })" ) ==
           "1:36: /: child is not a key of an entity (keys: name, components, children)" );
    CHECK( f.Why( R"({ "version": 1, "root": { "components": { "health": {}, "health": {} } } })" ) ==
           "1:67: /: health given twice" );
    CHECK( f.Why( R"({ "version": 2, "root": {} })" ) == "1:14: version: 2 is not a version this engine reads (1)" );
    CHECK( f.Why( R"({ "root": {} })" ) == "no version: a scene file says \"version\": 1" );
    CHECK( f.Why( R"({ "version": 1 })" ) == "no root: a scene file has \"root\": { ... }" );
    CHECK( f.Why( R"({ "version": 1, "root": {}, "extra": 1 })" ) ==
           "1:38: the file: extra is not a key of a scene (keys: version, root)" );
    CHECK( f.Why( "[" ) == "1:2: a value expected, got the end" );
}

TEST_CASE( "One entity goes to JSON and back under a parent, its name made unique there" )
{
    const Fixture f;
    Scene scene = f.Made();
    const Entity props = *TryResolve( scene, EntityPath{ Make( "/props" ) } );
    const Entity chair = *TryResolve( scene, EntityPath{ Make( "/props/chair" ) } );
    const auto jsonMaybe = TryEntityToJson( scene, chair, f.mComponentTypes );
    REQUIRE_MESSAGE( jsonMaybe, jsonMaybe.error() );
    const auto madeMaybe = TryEntityFromJson( scene, *jsonMaybe, props, f.mComponentTypes, 0 );
    REQUIRE_MESSAGE( madeMaybe, madeMaybe.error() );
    CHECK( PathOf( scene, *madeMaybe ).mPath.View() == "/props/chair_2" );
    CHECK( scene.Get<Health>( *madeMaybe ).mValue == 50 );
    CHECK( ChildrenOf( scene, props ).front() == *madeMaybe );

    // A wrong one leaves nothing behind.
    const size_t count = scene.Count();
    const auto wrongMaybe = Json::TryParse( R"({ "name": "box", "components": { "health": { "value": "lots" } } })" );
    REQUIRE( wrongMaybe );
    CHECK_FALSE( TryEntityFromJson( scene, *wrongMaybe, props, f.mComponentTypes ) );
    CHECK( scene.Count() == count );
    CHECK( ChildrenOf( scene, props ).size() == 2 );
}

TEST_CASE( "A field the writer leaves out is not a field a file may name" )
{
    Fixture f;
    // The rebuilt field is not in the component's declaration, so a file that
    // names it is refused - although the same record handed to reflection
    // itself is taken: see "A field the writer said not to save is still read
    // when a file names it" in the reflection tests.
    CHECK(
        f.Why(
             R"({ "version": 1, "root": { "children": [ { "name": "lamp", "components": { "light": { "cache": 9 } } } ] } })" )
            .ends_with(
                "/lamp: light.cache: not a field (fields: color, brightness, flickers, steps, target, props)" ) );
    CHECK(
        f.Why(
             R"({ "version": 1, "root": { "children": [ { "name": "lamp", "components": { "light": { "nope": 1 } } } ] } })" )
            .ends_with(
                "/lamp: light.nope: not a field (fields: color, brightness, flickers, steps, target, props)" ) );
    // A whole number field says what it has to be, and a file that leaves a
    // field out is not told about it: the default from the type stands.
    CHECK(
        f.Why(
             R"({ "version": 1, "root": { "children": [ { "name": "lamp", "components": { "light": { "flickers": 2.5 } } } ] } })" )
            .ends_with( "/lamp: light.flickers: an integer expected, got 2.5" ) );
    // A path a field points at is not checked: the file loads, and the link
    // leads nowhere only when something asks for it.
    const Scene pointed = f.Load(
        R"({ "version": 1, "root": { "children": [ { "name": "lamp", "components": { "light": { "target": "/gone" } } } ] } })" );
    const auto pointedLightMaybe = pointed.TryGet<Light>( *TryResolve( pointed, EntityPath{ Make( "/lamp" ) } ) );
    REQUIRE( pointedLightMaybe );
    CHECK( pointedLightMaybe->mTarget == EntityPath{ Make( "/gone" ) } );
    CHECK_FALSE( TryResolve( pointed, pointedLightMaybe->mTarget ) );
}

TEST_CASE( "Two children whose names read the same are one name, and spaces are part of a name" )
{
    Fixture f;
    // "cafe" with a separate accent mark and "cafÃ©" with the letter are the same
    // name: the file that spells it two ways has the same child twice.
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \"caf\\u00e9\" }, "
                  "{ \"name\": \"cafe\\u0301\" } ] } }" )
               .ends_with( "/: a second child named \"caf\xC3\xA9\"" ) );
    // A name is not trimmed: "chair " and "chair" are two children, and the
    // path keeps the space.
    const Scene spaced = f.Load( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \"chair\" }, "
                                 "{ \"name\": \"chair \" } ] } }" );
    CHECK( ChildrenOf( spaced, spaced.Root() ).size() == 2 );
    CHECK( TryResolve( spaced, EntityPath{ Make( "/chair " ) } ) );
    CHECK( f.Save( spaced ) == f.Save( f.Load( f.Save( spaced ) ) ) );
    // A leading dot is a name like any other; only "." and ".." themselves are
    // not names, nor is an empty one.
    const Scene dotted = f.Load( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \".hidden\" } ] } }" );
    CHECK( TryResolve( dotted, EntityPath{ Make( "/.hidden" ) } ) );
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \"\" } ] } }" )
               .ends_with( "\"\" is not a name an entity can have: not empty, no /, not . or .." ) );
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \".\" } ] } }" )
               .ends_with( "\".\" is not a name an entity can have: not empty, no /, not . or .." ) );
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": [ { \"name\": \"..\" } ] } }" )
               .ends_with( "\"..\" is not a name an entity can have: not empty, no /, not . or .." ) );
    // A child that is not an object at all is reported as having no name, which
    // is not what is wrong with it.
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": [ 3 ] } }" ).ends_with( "/: a child with no name" ) );
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"children\": null } }" )
               .ends_with( "/: children - an array - expected, got null" ) );
    // A component key that is not a name at all reads as "not a component".
    CHECK( f.Why( "{ \"version\": 1, \"root\": { \"components\": { \"Light\": {} } } }" )
               .ends_with( "/: Light is not a component (components: light, health)" ) );
}

TEST_CASE( "A version written as a fraction, a text, or too big to be a number" )
{
    Fixture f;
    // 1.0 is the same number: a person writing it by hand gets the file read.
    CHECK( f.Load( "{ \"version\": 1.0, \"root\": {} }" ).Count() == 1 );
    CHECK( f.Why( "{ \"version\": 1.5, \"root\": {} }" ).ends_with( "version: a whole number expected, got 1.5" ) );
    CHECK( f.Why( "{ \"version\": \"1\", \"root\": {} }" ).ends_with( "version: a whole number expected, got \"1\"" ) );
    CHECK( f.Why( "{ \"version\": true, \"root\": {} }" ).ends_with( "version: a whole number expected, got true" ) );
    CHECK( f.Why( "{ \"version\": null, \"root\": {} }" ).ends_with( "version: a whole number expected, got null" ) );
    // A whole number as big as an int holds is still not a version this engine
    // reads; the number is written back the way the file's own number text goes.
    CHECK( f.Why( "{ \"version\": 2147483647, \"root\": {} }" )
               .ends_with( "version: 2147483647 is not a version this engine reads (1)" ) );
    CHECK( f.Why( "{ \"version\": -0, \"root\": {} }" )
               .ends_with( "version: -0 is not a version this engine reads (1)" ) );
    // The same key written twice: the first of them is the one that counts, and
    // the file is still written back with both.
    const Scene twice =
        f.Load( "{ \"version\": 1, \"root\": {}, \"root\": { \"children\": [ { \"name\": \"ghost\" } ] } }" );
    CHECK( twice.Count() == 1 );
    CHECK_FALSE( TryResolve( twice, EntityPath{ Make( "/ghost" ) } ) );
}

TEST_CASE( "A tree deeper than a file holds is refused as it is saved" )
{
    Fixture f;
    // Each level of the tree is two levels of the file, a child in its
    // parent's "children", and the reader stops past 256: what saves, loads.
    Scene scene;
    Entity at = scene.Root();
    for ( int i = 0; i < 127; ++i )
        at = CreateChild( scene, at, Make( "step" ) );
    const auto savedMaybe = TrySaveScene( scene, f.mComponentTypes );
    REQUIRE_MESSAGE( savedMaybe, savedMaybe.error() );
    const auto loadedMaybe = TryLoadScene( *savedMaybe, f.mComponentTypes );
    REQUIRE_MESSAGE( loadedMaybe, loadedMaybe.error() );
    CHECK( loadedMaybe->Count() == scene.Count() );

    // One level more has its name past what the reader takes.
    const Entity deepest = CreateChild( scene, at, Make( "step" ) );
    CHECK( TrySaveScene( scene, f.mComponentTypes ).error() ==
           format( "{}: nested deeper than a file holds (256 levels)", Describe( scene, deepest ) ) );
    // A component counts too: on the last level that fits, its fields go past.
    scene.Destroy( deepest );
    scene.Add<Health>( at );
    CHECK( TrySaveScene( scene, f.mComponentTypes ).error() ==
           format( "{}: nested deeper than a file holds (256 levels)", Describe( scene, at ) ) );
}

TEST_CASE( "A number a file cannot hold is refused as it is saved" )
{
    const Fixture f;
    // A file has no word for NaN or infinity; written as null, it would not
    // load again.
    for ( const auto& [broken, text] :
          { pair{ f32( NAN ), "nan" }, pair{ f32( INFINITY ), "inf" }, pair{ -f32( INFINITY ), "-inf" } } )
    {
        Scene scene = f.Made();
        scene.Get<Light>( *TryResolve( scene, EntityPath{ Make( "/lamp" ) } ) ).mBrightness = broken;
        CHECK( TrySaveScene( scene, f.mComponentTypes ).error() ==
               format( "/lamp: light.brightness: {} is a number a file cannot hold", text ) );
    }
}

TEST_CASE( "One entity written and read back, asked about where it cannot be" )
{
    const Fixture f;
    Scene scene = f.Made();
    const Entity loose = scene.Create();
    CHECK_THROWS_WITH_AS( (void)TryEntityToJson( scene, scene.Root(), f.mComponentTypes ),
                          doctest::Contains( "TryEntityToJson of /: an entity under the root is written" ),
                          logic_error );
    CHECK_THROWS_WITH_AS( (void)TryEntityToJson( scene, loose, f.mComponentTypes ),
                          doctest::Contains( "not in the tree): an entity under the root is written" ), logic_error );

    // A parent outside the tree is a mistake in the code, not in the file: it
    // throws rather than tell a person.
    const auto jsonMaybe = Json::TryParse( R"({ "name": "box" })" );
    REQUIRE( jsonMaybe );
    CHECK_THROWS_WITH_AS( (void)TryEntityFromJson( scene, *jsonMaybe, loose, f.mComponentTypes ),
                          doctest::Contains( "AddToTree on entity" ), logic_error );
    // A child of the root written alone keeps its name and nothing of where it
    // hangs: reading it back under another parent is what gives it a place.
    const size_t count = scene.Count();
    const Entity lamp = *TryResolve( scene, EntityPath{ Make( "/lamp" ) } );
    const auto lampJsonMaybe = TryEntityToJson( scene, lamp, f.mComponentTypes );
    REQUIRE_MESSAGE( lampJsonMaybe, lampJsonMaybe.error() );
    const auto underPropsMaybe = TryEntityFromJson(
        scene, *lampJsonMaybe, *TryResolve( scene, EntityPath{ Make( "/props" ) } ), f.mComponentTypes );
    REQUIRE_MESSAGE( underPropsMaybe, underPropsMaybe.error() );
    CHECK( PathOf( scene, *underPropsMaybe ).mPath.View() == "/props/lamp" );
    // The copy points at the same target the original did, which is now
    // a path that still leads somewhere - and the two lamps are two entities.
    CHECK( scene.Get<Light>( *underPropsMaybe ).mTarget == EntityPath{ Make( "/props/chair" ) } );
    CHECK( scene.Count() == count + 1 );
}
