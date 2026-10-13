// Field paths: "shadow.bias", "steps[2]", "props.waves[1].count" - parsed
// once, then read and written through fields, properties, lists and the
// Values a script keeps; and Value to a field's own type and back.
#include "bubble/reflection/reflection.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Shadow
{
    f32 mBias = 0.1f;
    f32 mSoftness = 1;
};

struct Light
{
    vec3 mColor{ 1, 1, 1 };
    f32 mBrightness = 1;
    i32 mFlickers = 0;
    Shadow mShadow;
    vector<f32> mSteps{ 1, 2, 3 };
    Value mProps;
    // Rebuilt, never saved.
    f32 mCache = 0;

    Shadow FarShadow() const { return mFar; }
    void SetFarShadow( Shadow shadow )
    {
        mFar = shadow;
        ++mFarSets;
    }

    Shadow mFar;
    int mFarSets = 0;
};

Text Make( string_view utf8 )
{
    return *Text::From( utf8 );
}

FieldPath Path( string_view text )
{
    auto path = FieldPath::Parse( text );
    REQUIRE_MESSAGE( path, path.error() );
    return *path;
}

struct Fixture
{
    Reflection mReflection;
    Light mLight;
    MetaAny mObject;

    Fixture()
    {
        mReflection.Type<Shadow>( "shadow" ).Field<&Shadow::mBias>( "bias" ).Field<&Shadow::mSoftness>( "softness" );
        mReflection.Type<Light>( "light" )
            .Field<&Light::mColor>( "color" )
            .Field<&Light::mBrightness>( "brightness" )
            .Field<&Light::mFlickers>( "flickers" )
            .Field<&Light::mShadow>( "shadow" )
            .Field<&Light::mSteps>( "steps" )
            .Field<&Light::mProps>( "props" )
            .Field<&Light::mCache>( "cache", { .mFlags = FieldInfo::Transient } )
            .Property<&Light::SetFarShadow, &Light::FarShadow>( "far_shadow" );

        REQUIRE( Name::From( "speed" ) );
        REQUIRE( Name::From( "waves" ) );
        REQUIRE( Name::From( "count" ) );
        mLight.mProps = Value::Record{
            { "speed"_n, 2 },
            { "waves"_n, Value::List{ Value::Record{ { "count"_n, 3 } } } },
        };
        mObject = mReflection.Ref( mLight );
    }

    expected<void, string> Set( string_view path, MetaAny value )
    {
        return mReflection.SetField( mObject, Path( path ), std::move( value ) );
    }
};
}

TEST_CASE( "A field path is parsed once and says what is wrong with it" )
{
    CHECK( Path( "shadow.bias" ).ToString() == "shadow.bias" );
    CHECK( Path( "steps[2]" ).Steps().size() == 2 );
    CHECK( get<size_t>( Path( "steps[2]" ).Steps()[1] ) == 2 );
    CHECK( Path( "props.waves[1].count" ).ToString() == "props.waves[1].count" );

    CHECK( FieldPath::Parse( "" ).error() == "an empty field path" );
    CHECK( FieldPath::Parse( "shadow." ).error() == "shadow.: an empty step" );
    CHECK( FieldPath::Parse( "shadow..bias" ).error() == "shadow..bias: an empty step" );
    CHECK( FieldPath::Parse( "Shadow" ).error().starts_with( "Shadow: 'Shadow' is not a name" ) );
    CHECK( FieldPath::Parse( "steps[0]" ).error() == "steps[0]: a position is a whole number from 1, not '0'" );
    CHECK( FieldPath::Parse( "steps[x]" ).error() == "steps[x]: a position is a whole number from 1, not 'x'" );
    CHECK( FieldPath::Parse( "steps[2" ).error() == "steps[2: [ without ]" );
    CHECK( FieldPath::Parse( "steps[2]x" ).error() == "steps[2]x: 'x' after a position, where . or [ goes" );
}

TEST_CASE( "A path reads through fields and lists, by reference" )
{
    Fixture f;
    auto bias = f.mReflection.GetField( f.mObject, Path( "shadow.bias" ) );
    REQUIRE( bias );
    CHECK( bias->cast<f32>() == 0.1f );
    // A reference: writing through it writes the light.
    bias->cast<f32&>() = 0.3f;
    CHECK( f.mLight.mShadow.mBias == 0.3f );

    CHECK( f.mReflection.GetField( f.mObject, Path( "steps[2]" ) )->cast<f32>() == 2.0f );
}

TEST_CASE( "A set deep in an object reports every field it passed, the innermost first" )
{
    Fixture f;
    vector<string> heard;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Shadow>(), "bias"_n,
                                  [&]( MetaAny& ) { heard.push_back( "shadow.bias" ); } );
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "shadow"_n,
                                  [&]( MetaAny& ) { heard.push_back( "light.shadow" ); } );
    REQUIRE( f.Set( "shadow.bias", MetaAny( 0.5f ) ) );
    CHECK( f.mLight.mShadow.mBias == 0.5f );
    CHECK( heard == vector<string>{ "shadow.bias", "light.shadow" } );
}

TEST_CASE( "A path through a property changes a copy and sets it back" )
{
    Fixture f;
    int heard = 0;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "far_shadow"_n, [&]( MetaAny& ) { ++heard; } );
    REQUIRE( f.Set( "far_shadow.softness", MetaAny( 4.0f ) ) );
    CHECK( f.mLight.mFar.mSoftness == 4.0f );
    CHECK( f.mLight.mFarSets == 1 );
    CHECK( heard == 1 );
}

TEST_CASE( "A path sets an element of a list, and says when there is none" )
{
    Fixture f;
    REQUIRE( f.Set( "steps[3]", MetaAny( 9 ) ) );
    CHECK( f.mLight.mSteps == vector<f32>{ 1, 2, 9 } );
    CHECK( f.Set( "steps[5]", MetaAny( 1.0f ) ).error() == "light.steps[5]: no element 5, there are 3" );
    CHECK( f.Set( "brightness[1]", MetaAny( 1.0f ) ).error() == "light.brightness[1]: not a list, it is a number" );
    CHECK( f.Set( "shadow.blur", MetaAny( 1.0f ) ).error() ==
           "light.shadow.blur: shadow has no field blur (fields: bias, softness)" );
    CHECK( f.Set( "shadow.bias", MetaAny( string( "soft" ) ) ).error() ==
           "light.shadow.bias: number expected, got text" );
}

TEST_CASE( "A path goes on into a script's props, a Value, and keeps each slot's kind" )
{
    Fixture f;
    int heard = 0;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "props"_n, [&]( MetaAny& ) { ++heard; } );

    REQUIRE( f.Set( "props.speed", MetaAny( 5 ) ) );
    REQUIRE( f.Set( "props.waves[1].count", MetaAny( 4 ) ) );
    CHECK( f.mReflection.GetField( f.mObject, Path( "props.speed" ) )->cast<const Value&>() == Value( 5 ) );
    CHECK( f.mReflection.GetField( f.mObject, Path( "props.waves[1].count" ) )->cast<const Value&>() == Value( 4 ) );
    CHECK( heard == 2 );

    CHECK( f.Set( "props.speed", MetaAny( string( "fast" ) ) ).error() ==
           "light.props.speed: number expected, got \"fast\"" );
    REQUIRE( Name::From( "sped" ) );
    CHECK( f.Set( "props.sped", MetaAny( 1 ) ).error() == "light.props.sped: no field sped (fields: speed, waves)" );
    CHECK( f.Set( "props.waves[2].count", MetaAny( 1 ) ).error() == "light.props.waves[2]: no element 2, there are 1" );
}

TEST_CASE( "A Value given for a C++ field is made into the field's type" )
{
    Fixture f;
    REQUIRE( f.Set( "color", MetaAny( Value( vec3( 0, 1, 0 ) ) ) ) );
    CHECK( f.mLight.mColor == vec3( 0, 1, 0 ) );
    REQUIRE( f.Set( "flickers", MetaAny( Value( 3 ) ) ) );
    CHECK( f.mLight.mFlickers == 3 );
    CHECK( f.Set( "flickers", MetaAny( Value( 2.5 ) ) ).error() == "light.flickers: integer expected, got 2.5" );
}

TEST_CASE( "An object becomes a Value and comes back, its transient fields left out" )
{
    Fixture f;
    f.mLight.mBrightness = 3;
    f.mLight.mShadow.mBias = 0.25f;
    f.mLight.mCache = 7;
    const auto value = f.mReflection.ToValue( f.mObject );
    REQUIRE( value );
    const auto record = value->As<Value::Record>();
    REQUIRE( record );
    for ( const ValueField& field : *record )
        CHECK( field.mName != "cache"_n );

    const auto made = f.mReflection.FromValue( *value, f.mReflection.TypeOf<Light>() );
    REQUIRE_MESSAGE( made, made.error() );
    const Light& copy = made->cast<const Light&>();
    CHECK( copy.mBrightness == 3.0f );
    CHECK( copy.mShadow.mBias == 0.25f );
    CHECK( copy.mSteps == f.mLight.mSteps );
    CHECK( copy.mProps == f.mLight.mProps );
    CHECK( copy.mCache == 0.0f );

    const Value wrong = Value::Record{ { "brightness"_n, Value( Make( "bright" ) ) } };
    CHECK( f.mReflection.FromValue( wrong, f.mReflection.TypeOf<Light>() ).error() ==
           "light.brightness: number expected, got \"bright\"" );
}
