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
    return *Text::TryFrom( utf8 );
}

FieldPath Path( string_view text )
{
    auto pathMaybe = FieldPath::TryParse( text );
    REQUIRE_MESSAGE( pathMaybe, pathMaybe.error() );
    return *pathMaybe;
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

        REQUIRE( Name::TryFrom( "speed" ) );
        REQUIRE( Name::TryFrom( "waves" ) );
        REQUIRE( Name::TryFrom( "count" ) );
        mLight.mProps = Value::Record{
            { "speed"_n, 2 },
            { "waves"_n, Value::List{ Value::Record{ { "count"_n, 3 } } } },
        };
        mObject = mReflection.Ref( mLight );
    }

    expected<void, string> TrySet( string_view path, MetaAny value )
    {
        return mReflection.TrySetField( mObject, Path( path ), std::move( value ) );
    }
};
}

TEST_CASE( "A field path is parsed once and says what is wrong with it" )
{
    CHECK( Path( "shadow.bias" ).ToString() == "shadow.bias" );
    CHECK( Path( "steps[2]" ).Steps().size() == 2 );
    CHECK( get<size_t>( Path( "steps[2]" ).Steps()[1] ) == 2 );
    CHECK( Path( "props.waves[1].count" ).ToString() == "props.waves[1].count" );

    CHECK( FieldPath::TryParse( "" ).error() == "an empty field path" );
    CHECK( FieldPath::TryParse( "shadow." ).error() == "shadow.: an empty step" );
    CHECK( FieldPath::TryParse( "shadow..bias" ).error() == "shadow..bias: an empty step" );
    CHECK( FieldPath::TryParse( "Shadow" ).error().starts_with( "Shadow: 'Shadow' is not a name" ) );
    CHECK( FieldPath::TryParse( "steps[0]" ).error() == "steps[0]: a position is a whole number from 1, not '0'" );
    CHECK( FieldPath::TryParse( "steps[x]" ).error() == "steps[x]: a position is a whole number from 1, not 'x'" );
    CHECK( FieldPath::TryParse( "steps[2" ).error() == "steps[2: [ without ]" );
    CHECK( FieldPath::TryParse( "steps[2]x" ).error() == "steps[2]x: 'x' after a position, where . or [ goes" );
}

TEST_CASE( "Path text keeps the shape of a way in, not the way it was typed" )
{
    // A leading zero is the same position, and the path says it back the way
    // the engine spells numbers.
    CHECK( *FieldPath::TryParse( "steps[02]" ) == Path( "steps[2]" ) );
    CHECK( Path( "steps[02]" ).ToString() == "steps[2]" );
    // More than one position in a row is a walk into a list of lists.
    CHECK( Path( "steps[2][1]" ).Steps().size() == 3 );
    CHECK( Path( "steps[2][1]" ).ToString() == "steps[2][1]" );

    // A position is a whole number from 1: nothing, a space, a sign, or a
    // number too big for a position is the same error naming what was typed.
    CHECK( FieldPath::TryParse( "steps[]" ).error() == "steps[]: a position is a whole number from 1, not ''" );
    CHECK( FieldPath::TryParse( "steps[ 2]" ).error() == "steps[ 2]: a position is a whole number from 1, not ' 2'" );
    CHECK( FieldPath::TryParse( "steps[-2]" ).error() == "steps[-2]: a position is a whole number from 1, not '-2'" );
    CHECK( FieldPath::TryParse( "steps[+2]" ).error() == "steps[+2]: a position is a whole number from 1, not '+2'" );
    CHECK( FieldPath::TryParse( "steps[99999999999999999999]" )
               .error()
               .starts_with( "steps[99999999999999999999]: a position is a whole number from 1" ) );
    // A number of the wrong shape is refused before it is looked at, so a path
    // that survives parsing never asks for position zero.
    CHECK_FALSE( FieldPath::TryParse( "steps[0]" ) );

    // What a person left out or typed too much of, said by the whole path.
    CHECK( FieldPath::TryParse( "[2]" ).error() == "[2]: an empty step" );
    CHECK( FieldPath::TryParse( "shadow.bias." ).error() == "shadow.bias.: an empty step" );
    CHECK( FieldPath::TryParse( "steps[2]]" ).error() == "steps[2]]: ']' after a position, where . or [ goes" );
    CHECK( FieldPath::TryParse( "steps [2]" ).error().starts_with( "steps [2]: 'steps ' is not a name" ) );
    CHECK( FieldPath::TryParse( "шаг" ).error().starts_with( "шаг: 'шаг' is not a name" ) );
}

TEST_CASE( "A path built while walking says the same as one typed, and cuts to a prefix" )
{
    const FieldPath built = FieldPath{}.Then( "shadow"_n ).Then( "bias"_n );
    CHECK( built == Path( "shadow.bias" ) );
    CHECK( built.ToString() == "shadow.bias" );
    // A name and a position are different steps, however they read.
    CHECK( FieldPath{}.Then( "a"_n ).Then( size_t( 1 ) ) == Path( "a[1]" ) );
    CHECK( Path( "a[1]" ) != Path( "a.b" ) );

    // What a message says got that far: a name and a position are separate
    // steps, so two steps is "props.waves" and three is the element.
    CHECK( Path( "props.waves[1].count" ).Prefix( 0 ).empty() );
    CHECK( Path( "props.waves[1].count" ).Prefix( 1 ) == "props" );
    CHECK( Path( "props.waves[1].count" ).Prefix( 2 ) == "props.waves" );
    CHECK( Path( "props.waves[1].count" ).Prefix( 3 ) == "props.waves[1]" );
    // Asking for more steps than there are is not a mistake: the whole path.
    CHECK( Path( "props.waves[1].count" ).Prefix( 99 ) == "props.waves[1].count" );

    // A walk into a list starts with a position, text a person cannot type:
    // Prefix writes "[2]" while TryParse refuses it at the front of a path.
    const FieldPath element = FieldPath{}.Then( size_t( 2 ) );
    CHECK( element.ToString() == "[2]" );
    CHECK_FALSE( FieldPath::TryParse( "[2]" ) );
}

TEST_CASE( "A step the value there cannot take says what is there instead" )
{
    Fixture f;
    // A field of something that is a plain number, not a record.
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props.speed.count" ) ).error() ==
           "light.props.speed.count: not a record, it is 2" );
    // A position in a record, and a position in a number.
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props[1]" ) ).error().find( "light.props[1]: not a list" ) !=
           string::npos );
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props.waves[1].count[1]" ) ).error() ==
           "light.props.waves[1].count[1]: not a list, it is 3" );
    // A field of a C++ number: a number has no fields to name.
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "brightness.deeper" ) ).error() ==
           "light.brightness.deeper: number has no field deeper (fields: )" );
    // A path with no steps at all is only ever built in code, and says so.
    CHECK( f.mReflection.TrySetField( f.mObject, FieldPath{}, MetaAny( 1 ) ).error() == "an empty field path" );
}

TEST_CASE( "A record that leaves fields out fills the rest from the type's own default" )
{
    Fixture f;
    // A property goes through its setter, so the whole Shadow is replaced: the
    // field the record does not name comes from a default-constructed Shadow,
    // not from what the light held before.
    f.mLight.mFar.mBias = 9;
    REQUIRE( f.TrySet( "far_shadow", MetaAny( Value( Value::Record{ { "softness"_n, 6 } } ) ) ) );
    CHECK( f.mLight.mFar.mSoftness == 6.0f );
    CHECK( f.mLight.mFar.mBias == 0.1f );
    CHECK( f.mLight.mFarSets == 1 );

    // What the path returned past a property is a copy, not a link: writing
    // into it changes nothing, and only a set through the path does.
    auto softnessMaybe = f.mReflection.TryGetField( f.mObject, Path( "far_shadow.softness" ) );
    REQUIRE( softnessMaybe );
    softnessMaybe->cast<f32&>() = 3.0f;
    CHECK( f.mLight.mFar.mSoftness == 6.0f );
    CHECK( f.mLight.mFarSets == 1 );
}

TEST_CASE( "A field the writer said not to save is still read when a file names it" )
{
    Fixture f;
    // Transient says what TryToValue leaves out, not what is refused: a level
    // file that happens to name the cache has it read back.
    const auto savedMaybe = f.mReflection.TryToValue( f.mReflection.Ref( f.mLight ) );
    REQUIRE_MESSAGE( savedMaybe, savedMaybe.error() );
    for ( const ValueField& field : *savedMaybe->TryAs<Value::Record>() )
        CHECK( field.mName != "cache"_n );

    const Value withCache = Value::Record{ { "cache"_n, 9 } };
    const auto madeMaybe = f.mReflection.TryFromValue( withCache, f.mReflection.TypeOf<Light>() );
    REQUIRE_MESSAGE( madeMaybe, madeMaybe.error() );
    CHECK( madeMaybe->cast<const Light&>().mCache == 9.0f );
    // A record naming a field nobody described is refused, naming what there is.
    REQUIRE( Name::TryFrom( "lumens" ) );
    CHECK( f.mReflection.TryFromValue( Value::Record{ { "lumens"_n, 9 } }, f.mReflection.TypeOf<Light>() )
               .error()
               .starts_with( "light.lumens: light has no field lumens (fields:" ) );
    // An empty record is a whole object: every field keeps its own default.
    const auto plainMaybe = f.mReflection.TryFromValue( Value::Record{}, f.mReflection.TypeOf<Light>() );
    REQUIRE_MESSAGE( plainMaybe, plainMaybe.error() );
    CHECK( plainMaybe->cast<const Light&>().mBrightness == 1.0f );
    CHECK( plainMaybe->cast<const Light&>().mSteps == vector<f32>{ 1, 2, 3 } );
    // A number given for an object is said plainly.
    CHECK( f.mReflection.TryFromValue( Value( 3 ), f.mReflection.TypeOf<Light>() ).error() ==
           "light: light expected, got 3" );
}

TEST_CASE( "A described type says what shape its fields have in a file" )
{
    Fixture f;
    const auto shadowMaybe = f.mReflection.TryValueTypeOf( f.mReflection.TypeOf<Shadow>() );
    REQUIRE_MESSAGE( shadowMaybe, shadowMaybe.error() );
    const auto lightMaybe = f.mReflection.TryValueTypeOf( f.mReflection.TypeOf<Light>() );
    REQUIRE_MESSAGE( lightMaybe, lightMaybe.error() );
    // A described type is a record of its fields' names: the cache the owner
    // rebuilds is not in it, and a Property counts as a field like any other.
    CHECK( shadowMaybe->Describe() == "a record (bias, softness)" );
    CHECK( lightMaybe->Describe() == "a record (color, brightness, flickers, shadow, steps, props, far_shadow)" );
}

TEST_CASE( "A number handed over as a C++ number is cut to fit an integer field" )
{
    Fixture f;
    // A file's number comes as a Value, and an integer field says no to a
    // fraction. What a script hands over is a C++ number, and the language's
    // own conversion cuts it: 2.5 becomes 2 with nothing said.
    CHECK( f.TrySet( "flickers", MetaAny( Value( 2.5 ) ) ).error() == "light.flickers: integer expected, got 2.5" );
    REQUIRE( f.TrySet( "flickers", MetaAny( 2.5f ) ) );
    CHECK( f.mLight.mFlickers == 2 );
    // The same through a path: the value converts, and the cut is silent.
    REQUIRE( f.TrySet( "shadow.bias", MetaAny( 1 ) ) );
    CHECK( f.mLight.mShadow.mBias == 1.0f );
}

TEST_CASE( "A whole record replaces the one a script's props held" )
{
    Fixture f;
    // The kind of the slot holds - a record for a record - and the old one is
    // gone, fields it had and the new one does not name included.
    REQUIRE(
        f.TrySet( "props.waves[1]", MetaAny( Value( Value::Record{ { "bias"_n, 0.5 }, { "softness"_n, 2 } } ) ) ) );
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props.waves[1].bias" ) )->cast<const Value&>() ==
           Value( 0.5 ) );
    CHECK( f.TrySet( "props.waves[1].count", MetaAny( 1 ) ).error() ==
           "light.props.waves[1].count: no field count (fields: bias, softness)" );
    // A field that is a list of objects cannot take one object's fields.
    CHECK( f.TrySet( "props.waves[1].bias.softness", MetaAny( 1 ) ).error() ==
           "light.props.waves[1].bias.softness: not a record, it is 0.5" );
}

TEST_CASE( "A path reads through fields and lists, by reference" )
{
    Fixture f;
    auto biasMaybe = f.mReflection.TryGetField( f.mObject, Path( "shadow.bias" ) );
    REQUIRE( biasMaybe );
    CHECK( biasMaybe->cast<f32>() == 0.1f );
    // A reference: writing through it writes the light.
    biasMaybe->cast<f32&>() = 0.3f;
    CHECK( f.mLight.mShadow.mBias == 0.3f );

    CHECK( f.mReflection.TryGetField( f.mObject, Path( "steps[2]" ) )->cast<f32>() == 2.0f );
}

TEST_CASE( "A set deep in an object reports every field it passed, the innermost first" )
{
    Fixture f;
    vector<string> heard;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Shadow>(), "bias"_n,
                                  [&]( MetaAny& ) { heard.push_back( "shadow.bias" ); } );
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "shadow"_n,
                                  [&]( MetaAny& ) { heard.push_back( "light.shadow" ); } );
    REQUIRE( f.TrySet( "shadow.bias", MetaAny( 0.5f ) ) );
    CHECK( f.mLight.mShadow.mBias == 0.5f );
    CHECK( heard == vector<string>{ "shadow.bias", "light.shadow" } );
}

TEST_CASE( "A path through a property changes a copy and sets it back" )
{
    Fixture f;
    int heard = 0;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "far_shadow"_n, [&]( MetaAny& ) { ++heard; } );
    REQUIRE( f.TrySet( "far_shadow.softness", MetaAny( 4.0f ) ) );
    CHECK( f.mLight.mFar.mSoftness == 4.0f );
    CHECK( f.mLight.mFarSets == 1 );
    CHECK( heard == 1 );
}

TEST_CASE( "A path sets an element of a list, and says when there is none" )
{
    Fixture f;
    REQUIRE( f.TrySet( "steps[3]", MetaAny( 9 ) ) );
    CHECK( f.mLight.mSteps == vector<f32>{ 1, 2, 9 } );
    CHECK( f.TrySet( "steps[5]", MetaAny( 1.0f ) ).error() == "light.steps[5]: no element 5, there are 3" );
    CHECK( f.TrySet( "brightness[1]", MetaAny( 1.0f ) ).error() == "light.brightness[1]: not a list, it is a number" );
    CHECK( f.TrySet( "shadow.blur", MetaAny( 1.0f ) ).error() ==
           "light.shadow.blur: shadow has no field blur (fields: bias, softness)" );
    CHECK( f.TrySet( "shadow.bias", MetaAny( string( "soft" ) ) ).error() ==
           "light.shadow.bias: number expected, got text" );
}

TEST_CASE( "A path goes on into a script's props, a Value, and keeps each slot's kind" )
{
    Fixture f;
    int heard = 0;
    f.mReflection.OnFieldChanged( f.mReflection.TypeOf<Light>(), "props"_n, [&]( MetaAny& ) { ++heard; } );

    REQUIRE( f.TrySet( "props.speed", MetaAny( 5 ) ) );
    REQUIRE( f.TrySet( "props.waves[1].count", MetaAny( 4 ) ) );
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props.speed" ) )->cast<const Value&>() == Value( 5 ) );
    CHECK( f.mReflection.TryGetField( f.mObject, Path( "props.waves[1].count" ) )->cast<const Value&>() == Value( 4 ) );
    CHECK( heard == 2 );

    CHECK( f.TrySet( "props.speed", MetaAny( string( "fast" ) ) ).error() ==
           "light.props.speed: number expected, got \"fast\"" );
    REQUIRE( Name::TryFrom( "sped" ) );
    CHECK( f.TrySet( "props.sped", MetaAny( 1 ) ).error() == "light.props.sped: no field sped (fields: speed, waves)" );
    CHECK( f.TrySet( "props.waves[2].count", MetaAny( 1 ) ).error() ==
           "light.props.waves[2]: no element 2, there are 1" );
}

TEST_CASE( "A Value given for a C++ field is made into the field's type" )
{
    Fixture f;
    REQUIRE( f.TrySet( "color", MetaAny( Value( vec3( 0, 1, 0 ) ) ) ) );
    CHECK( f.mLight.mColor == vec3( 0, 1, 0 ) );
    REQUIRE( f.TrySet( "flickers", MetaAny( Value( 3 ) ) ) );
    CHECK( f.mLight.mFlickers == 3 );
    CHECK( f.TrySet( "flickers", MetaAny( Value( 2.5 ) ) ).error() == "light.flickers: integer expected, got 2.5" );
}

TEST_CASE( "An object becomes a Value and comes back, its transient fields left out" )
{
    Fixture f;
    f.mLight.mBrightness = 3;
    f.mLight.mShadow.mBias = 0.25f;
    f.mLight.mCache = 7;
    const auto valueMaybe = f.mReflection.TryToValue( f.mObject );
    REQUIRE( valueMaybe );
    const auto recordMaybe = valueMaybe->TryAs<Value::Record>();
    REQUIRE( recordMaybe );
    for ( const ValueField& field : *recordMaybe )
        CHECK( field.mName != "cache"_n );

    const auto madeMaybe = f.mReflection.TryFromValue( *valueMaybe, f.mReflection.TypeOf<Light>() );
    REQUIRE_MESSAGE( madeMaybe, madeMaybe.error() );
    const Light& copy = madeMaybe->cast<const Light&>();
    CHECK( copy.mBrightness == 3.0f );
    CHECK( copy.mShadow.mBias == 0.25f );
    CHECK( copy.mSteps == f.mLight.mSteps );
    CHECK( copy.mProps == f.mLight.mProps );
    CHECK( copy.mCache == 0.0f );

    const Value wrong = Value::Record{ { "brightness"_n, Value( Make( "bright" ) ) } };
    CHECK( f.mReflection.TryFromValue( wrong, f.mReflection.TypeOf<Light>() ).error() ==
           "light.brightness: number expected, got \"bright\"" );
}
