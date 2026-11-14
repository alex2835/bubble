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
    const auto typeMaybe = f.mReflection.TryFind( "light"_n );
    REQUIRE( typeMaybe );
    CHECK( *typeMaybe == f.mReflection.TypeOf<Light>() );
    CHECK( f.mReflection.NameOf( *typeMaybe ) == "light" );
    CHECK( string_view( Reflection::InfoOf( *typeMaybe ).mNote ) == "A point that shines in every direction." );

    vector<string> names;
    f.mReflection.ForEachField( *typeMaybe,
                                [&]( Name name, const MetaField& ) { names.push_back( name.ToString() ); } );
    CHECK( names == vector<string>{ "color", "brightness", "on", "range" } );

    CHECK( Reflection::InfoOf( typeMaybe->data( "color"_n.Hash() ) ).Has( FieldInfo::Color ) );
    CHECK( Reflection::InfoOf( typeMaybe->data( "brightness"_n.Hash() ) ).mMax == 10 );
    CHECK_FALSE( Reflection::InfoOf( typeMaybe->data( "on"_n.Hash() ) ).Has( FieldInfo::Color ) );
    CHECK_FALSE( f.mReflection.TryFind( "lamp"_n ) );
}

TEST_CASE( "A field is read and written by name, and a number converts" )
{
    Fixture f;
    REQUIRE( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( 3 ) ) );
    CHECK( f.mLight.mBrightness == 3.0f );
    REQUIRE( f.mReflection.TrySet( f.mObject, "color"_n, MetaAny( vec3( 1, 0, 0 ) ) ) );
    CHECK( f.mLight.mColor == vec3( 1, 0, 0 ) );

    const auto brightnessMaybe = f.mReflection.TryGet( f.mObject, "brightness"_n );
    REQUIRE( brightnessMaybe );
    CHECK( brightnessMaybe->cast<f32>() == 3.0f );
}

TEST_CASE( "A property goes through its setter" )
{
    Fixture f;
    REQUIRE( f.mReflection.TrySet( f.mObject, "range"_n, MetaAny( -5.0f ) ) );
    CHECK( f.mLight.Range() == 0.0f );
    CHECK( f.mReflection.TryGet( f.mObject, "range"_n )->cast<f32>() == 0.0f );
}

TEST_CASE( "A field that is not there or a value of the wrong kind is an error that says what there is" )
{
    Fixture f;
    // A typo nobody declared is shown by its hash.
    const string typo = f.mReflection.TryGet( f.mObject, "brightnes"_n ).error();
    CHECK( typo.starts_with( "light has no field #" ) );
    CHECK( typo.ends_with( " (fields: color, brightness, on, range)" ) );
    REQUIRE( Name::TryFrom( "intensity" ) );
    CHECK( f.mReflection.TrySet( f.mObject, "intensity"_n, MetaAny( 1.0f ) ).error() ==
           "light has no field intensity (fields: color, brightness, on, range)" );
    CHECK( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( string( "bright" ) ) ).error() ==
           "light.brightness: number expected, got text" );
    CHECK( f.mLight.mBrightness == 1.0f );
}

TEST_CASE( "A method is called by name, its arguments checked" )
{
    Fixture f;
    REQUIRE( f.mReflection.TryCall( f.mObject, "toggle"_n, {} ) );
    CHECK_FALSE( f.mLight.mOn );

    vector<MetaAny> args{ MetaAny( 2 ) };
    const auto brightenedMaybe = f.mReflection.TryCall( f.mObject, "brighten"_n, args );
    REQUIRE( brightenedMaybe );
    CHECK( brightenedMaybe->cast<f32>() == 3.0f );

    REQUIRE( Name::TryFrom( "dim" ) );
    CHECK( f.mReflection.TryCall( f.mObject, "dim"_n, {} ).error() ==
           "light has no method dim (methods: toggle, brighten)" );
    CHECK( f.mReflection.TryCall( f.mObject, "brighten"_n, {} ).error() == "light.brighten takes 1 argument, got 0" );
    vector<MetaAny> wrong{ MetaAny( string( "lots" ) ) };
    CHECK( f.mReflection.TryCall( f.mObject, "brighten"_n, wrong ).error() ==
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
    REQUIRE( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( 5.0f ) ) );
    REQUIRE( f.mReflection.TrySet( f.mObject, "color"_n, MetaAny( vec3( 0 ) ) ) );
    CHECK( heard == 1 );

    // A failed set changed nothing and tells nobody.
    CHECK_FALSE( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( string( "x" ) ) ) );
    CHECK( heard == 1 );

    // Code that changed the field itself says so.
    f.mReflection.NotifyChanged( f.mObject, "brightness"_n );
    CHECK( heard == 2 );

    f.mReflection.RemoveListener( listener );
    REQUIRE( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( 5.0f ) ) );
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
    REQUIRE( f.mReflection.TrySet( f.mObject, "on"_n, MetaAny( false ) ) );
    REQUIRE( f.mReflection.TrySet( f.mObject, "on"_n, MetaAny( true ) ) );
    CHECK( heard == 1 );
}

TEST_CASE( "Two reflections know nothing of each other, and a bad name is a bug" )
{
    Fixture f;
    Reflection other;
    CHECK_FALSE( other.TryFind( "light"_n ) );
    CHECK_FALSE( other.TypeOf<Light>().data( "color"_n.Hash() ) );
    CHECK_THROWS_WITH_AS( other.Type<Light>( "Light" ), doctest::Contains( "is not a name" ), logic_error );
}

TEST_CASE( "An object that came as const reads, and says no to being written or called" )
{
    Fixture f;
    const Light& constantLight = f.mLight;
    MetaAny constantObject = f.mReflection.Ref( constantLight );

    // Reading, through a field and through a path, is fine.
    CHECK( f.mReflection.TryGet( constantObject, "brightness"_n )->cast<f32>() == 1.0f );
    const auto pathMaybe = FieldPath::TryParse( "range" );
    REQUIRE_MESSAGE( pathMaybe, pathMaybe.error() );
    CHECK( f.mReflection.TryGetField( constantObject, *pathMaybe )->cast<f32>() == 10.0f );

    // Writing is refused once the field is known, by name and by path alike.
    CHECK( f.mReflection.TrySet( constantObject, "brightness"_n, MetaAny( 2.0f ) ).error() ==
           "light.brightness cannot be set here: the object is const" );
    CHECK( f.mReflection.TrySetField( constantObject, *pathMaybe, MetaAny( 2.0f ) ).error() ==
           "light.range cannot be set here: the object is const" );
    // A method that changes the object cannot run either; one that only reads
    // still does.
    CHECK( f.mReflection.TryCall( constantObject, "toggle"_n, {} ).error() ==
           "light.toggle cannot be called here: the object is const" );
    vector<MetaAny> args{ MetaAny( 2.0f ) };
    CHECK( f.mReflection.TryCall( constantObject, "brighten"_n, args ).error() ==
           "light.brighten cannot be called here: the object is const" );
    CHECK( f.mLight.mBrightness == 1.0f );
    CHECK( f.mReflection.TryGet( constantObject, "range"_n )->cast<f32>() == 10.0f );
    // A field nobody described is named before const is even asked about.
    REQUIRE( Name::TryFrom( "lumens" ) );
    CHECK( f.mReflection.TrySet( constantObject, "lumens"_n, MetaAny( 1.0f ) )
               .error()
               .starts_with( "light has no field lumens" ) );
}

TEST_CASE( "A number goes into a bool field the way C++ reads a number" )
{
    Fixture f;
    // Reflection converts with the language's own rules, so any number but
    // zero turns the switch on, and zero turns it off.
    REQUIRE( f.mReflection.TrySet( f.mObject, "on"_n, MetaAny( 0.0f ) ) );
    CHECK_FALSE( f.mLight.mOn );
    REQUIRE( f.mReflection.TrySet( f.mObject, "on"_n, MetaAny( 0.25f ) ) );
    CHECK( f.mLight.mOn );
    // The other way: a bool is the number 1 or 0.
    REQUIRE( f.mReflection.TrySet( f.mObject, "brightness"_n, MetaAny( false ) ) );
    CHECK( f.mLight.mBrightness == 0.0f );
    // Text is what does not convert, and the message says the field's type.
    CHECK( f.mReflection.TrySet( f.mObject, "on"_n, MetaAny( string( "yes" ) ) ).error() ==
           "light.on: bool expected, got text" );
    // A method that wants a number takes a bool too.
    vector<MetaAny> boolArg{ MetaAny( true ) };
    const auto brightenedMaybe = f.mReflection.TryCall( f.mObject, "brighten"_n, boolArg );
    REQUIRE_MESSAGE( brightenedMaybe, brightenedMaybe.error() );
    CHECK( brightenedMaybe->cast<f32>() == 1.0f );
}

namespace
{
struct Marker
{
    Name mKind;
    EntityPath mTarget;
    vec3 mOffset{ 0, 0, 0 };
    Color mTint;
};
}

TEST_CASE( "A text or a list of numbers in a Value is taken for the name, entity, vec3 or color a field is" )
{
    Reflection reflection;
    reflection.Type<Marker>( "marker" )
        .Field<&Marker::mKind>( "kind" )
        .Field<&Marker::mTarget>( "target" )
        .Field<&Marker::mOffset>( "offset" )
        .Field<&Marker::mTint>( "tint" );
    Marker marker;
    MetaAny object = reflection.Ref( marker );
    const auto set = [&]( string_view path, Value value ) {
        return reflection.TrySetField( object, *FieldPath::TryParse( path ), MetaAny( std::move( value ) ) );
    };

    REQUIRE( set( "kind", Value( *Text::TryFrom( "door" ) ) ) );
    CHECK( marker.mKind.ToString() == "door" );
    REQUIRE( set( "target", Value( *Text::TryFrom( "/props/door" ) ) ) );
    CHECK( marker.mTarget.mPath.View() == "/props/door" );
    REQUIRE( set( "offset", Value( Value::List{ 1, 2, 3 } ) ) );
    CHECK( marker.mOffset == vec3( 1, 2, 3 ) );
    REQUIRE( set( "tint", Value( Value::List{ 1, 0, 0 } ) ) );
    CHECK( marker.mTint == Color{ 1, 0, 0, 1 } );
    REQUIRE( set( "tint", Value( Value::List{ 0, 1, 0, 0.5 } ) ) );
    CHECK( marker.mTint == Color{ 0, 1, 0, 0.5f } );

    // What it cannot be is still refused, saying why.
    const auto badName = set( "kind", Value( *Text::TryFrom( "Door" ) ) );
    REQUIRE_FALSE( badName );
    CHECK( badName.error().find( "Door" ) != string::npos );
    CHECK_FALSE( set( "offset", Value( Value::List{ 1, 2 } ) ) );
    CHECK_FALSE( set( "tint", Value( Value::List{ 1, 2, 3, 4, 5 } ) ) );
    CHECK_FALSE( set( "target", Value( 3 ) ) );
    CHECK( marker.mKind.ToString() == "door" );
}
