// Reflection: types described once, then read, written and called by name,
// with "this field changed" for whoever cares about one field.
#include "bubble/reflection/reflection.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Light
{
    vec3 mColor{ 1, 1, 1 };
    f32 mBrightness = 1;
    bool mOn = true;

    f32 Range() const { return mRange; }
    void SetRange( f32 range ) { mRange = range < 0 ? 0 : range; }

    void Toggle() { mOn = not mOn; }
    f32 Brighten( f32 by ) { return mBrightness += by; }

private:
    f32 mRange = 10;
};

void Describe( Reflection& reflection )
{
    reflection.Type<Light>( "light" )
        .Note( "A point that shines in every direction." )
        .Field<&Light::mColor>( "color", { .mFlags = FieldInfo::Color } )
        .Field<&Light::mBrightness>( "brightness", { .mMin = 0, .mMax = 10 } )
        .Field<&Light::mOn>( "on" )
        .Property<&Light::SetRange, &Light::Range>( "range" )
        .Method<&Light::Toggle>( "toggle" )
        .Method<&Light::Brighten>( "brighten", { .mTooltip = "Adds to the brightness." } );
}

struct Fixture
{
    Reflection mReflection;
    Light mLight;
    MetaAny mObject;

    Fixture()
    {
        Describe( mReflection );
        mObject = mReflection.Ref( mLight );
    }
};
}

TEST_CASE( "A described type has a name and its fields in the order they were described" )
{
    Fixture f;
    const auto type = f.mReflection.Find( "light"_n );
    REQUIRE( type );
    CHECK( *type == f.mReflection.TypeOf<Light>() );
    CHECK( f.mReflection.NameOf( *type ) == "light" );
    CHECK( string_view( Reflection::InfoOf( *type ).mNote ) == "A point that shines in every direction." );

    vector<string> names;
    f.mReflection.ForEachField( *type, [&]( Name name, const MetaField& ) { names.push_back( name.ToString() ); } );
    CHECK( names == vector<string>{ "color", "brightness", "on", "range" } );

    CHECK( Reflection::InfoOf( type->data( "color"_n.Hash() ) ).Has( FieldInfo::Color ) );
    CHECK( Reflection::InfoOf( type->data( "brightness"_n.Hash() ) ).mMax == 10 );
    CHECK_FALSE( Reflection::InfoOf( type->data( "on"_n.Hash() ) ).Has( FieldInfo::Color ) );
    CHECK_FALSE( f.mReflection.Find( "lamp"_n ) );
}

TEST_CASE( "A field is read and written by name, and a number converts" )
{
    Fixture f;
    REQUIRE( f.mReflection.Set( f.mObject, "brightness"_n, MetaAny( 3 ) ) );
    CHECK( f.mLight.mBrightness == 3.0f );
    REQUIRE( f.mReflection.Set( f.mObject, "color"_n, MetaAny( vec3( 1, 0, 0 ) ) ) );
    CHECK( f.mLight.mColor == vec3( 1, 0, 0 ) );

    const auto brightness = f.mReflection.Get( f.mObject, "brightness"_n );
    REQUIRE( brightness );
    CHECK( brightness->cast<f32>() == 3.0f );
}

TEST_CASE( "A property goes through its setter" )
{
    Fixture f;
    REQUIRE( f.mReflection.Set( f.mObject, "range"_n, MetaAny( -5.0f ) ) );
    CHECK( f.mLight.Range() == 0.0f );
    CHECK( f.mReflection.Get( f.mObject, "range"_n )->cast<f32>() == 0.0f );
}

TEST_CASE( "A field that is not there or a value of the wrong kind is an error that says what there is" )
{
    Fixture f;
    // A typo nobody declared is shown by its hash.
    const string typo = f.mReflection.Get( f.mObject, "brightnes"_n ).error();
    CHECK( typo.starts_with( "light has no field #" ) );
    CHECK( typo.ends_with( " (fields: color, brightness, on, range)" ) );
    REQUIRE( Name::From( "intensity" ) );
    CHECK( f.mReflection.Set( f.mObject, "intensity"_n, MetaAny( 1.0f ) ).error() ==
           "light has no field intensity (fields: color, brightness, on, range)" );
    CHECK( f.mReflection.Set( f.mObject, "brightness"_n, MetaAny( string( "bright" ) ) ).error() ==
           "light.brightness: number expected, got text" );
    CHECK( f.mLight.mBrightness == 1.0f );
}

TEST_CASE( "A method is called by name, its arguments checked" )
{
    Fixture f;
    REQUIRE( f.mReflection.Call( f.mObject, "toggle"_n, {} ) );
    CHECK_FALSE( f.mLight.mOn );

    vector<MetaAny> args{ MetaAny( 2 ) };
    const auto brightened = f.mReflection.Call( f.mObject, "brighten"_n, args );
    REQUIRE( brightened );
    CHECK( brightened->cast<f32>() == 3.0f );

    REQUIRE( Name::From( "dim" ) );
    CHECK( f.mReflection.Call( f.mObject, "dim"_n, {} ).error() ==
           "light has no method dim (methods: toggle, brighten)" );
    CHECK( f.mReflection.Call( f.mObject, "brighten"_n, {} ).error() == "light.brighten takes 1 argument, got 0" );
    vector<MetaAny> wrong{ MetaAny( string( "lots" ) ) };
    CHECK( f.mReflection.Call( f.mObject, "brighten"_n, wrong ).error() ==
           "light.brighten: argument 1: number expected, got text" );
}

TEST_CASE( "A listener hears its own field change and no other" )
{
    Fixture f;
    int heard = 0;
    const auto listener =
        f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "brightness"_n, [&]( MetaAny& owner ) {
            CHECK( owner.cast<Light&>().mBrightness == 5.0f );
            ++heard;
        } );
    REQUIRE( f.mReflection.Set( f.mObject, "brightness"_n, MetaAny( 5.0f ) ) );
    REQUIRE( f.mReflection.Set( f.mObject, "color"_n, MetaAny( vec3( 0 ) ) ) );
    CHECK( heard == 1 );

    // A failed set changed nothing and tells nobody.
    CHECK_FALSE( f.mReflection.Set( f.mObject, "brightness"_n, MetaAny( string( "x" ) ) ) );
    CHECK( heard == 1 );

    // Code that changed the field itself says so.
    f.mReflection.NotifyChanged( f.mObject, "brightness"_n );
    CHECK( heard == 2 );

    f.mReflection.RemoveListener( listener );
    REQUIRE( f.mReflection.Set( f.mObject, "brightness"_n, MetaAny( 5.0f ) ) );
    CHECK( heard == 2 );
}

TEST_CASE( "A listener may take itself off while it is called" )
{
    Fixture f;
    int heard = 0;
    FieldListenerHandle self;
    self = f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "on"_n, [&]( MetaAny& ) {
        ++heard;
        f.mReflection.RemoveListener( self );
    } );
    REQUIRE( f.mReflection.Set( f.mObject, "on"_n, MetaAny( false ) ) );
    REQUIRE( f.mReflection.Set( f.mObject, "on"_n, MetaAny( true ) ) );
    CHECK( heard == 1 );
}

TEST_CASE( "Two reflections know nothing of each other, and a bad name is a bug" )
{
    Fixture f;
    Reflection other;
    CHECK_FALSE( other.Find( "light"_n ) );
    CHECK_FALSE( other.TypeOf<Light>().data( "color"_n.Hash() ) );
    CHECK_THROWS_WITH_AS( other.Type<Light>( "Light" ), doctest::Contains( "is not a name" ), logic_error );
}
