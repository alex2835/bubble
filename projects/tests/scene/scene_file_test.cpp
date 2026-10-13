// A scene as a file: saved as nested JSON, loaded back into the same scene,
// and a file that is wrong says where.
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene_file.hpp"
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
