// ComponentTypes: the components files hold, and a field set the way the
// scene's listeners hear of it.
#include "bubble/scene/component_types.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/format.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Shade
{
    f32 mSoftness = 0;
};

struct Lantern
{
    f32 mGlow = 1;
    Shade mShade;
};

struct Fields final : FieldListener
{
    vector<string> mHeard;

    void FieldChanged( Entity entity, Name component, Name field ) override
    {
        mHeard.push_back( format( "{} {}.{}", static_cast<u32>( entity ), component.ToString(), field.ToString() ) );
    }
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}
}

TEST_CASE( "A field set through ComponentTypes is heard by the scene's listeners, by its first step" )
{
    Reflection reflection;
    reflection.Type<Shade>( "shade" ).Field<&Shade::mSoftness>( "softness" );
    reflection.Type<Lantern>( "lantern" ).Field<&Lantern::mGlow>( "glow" ).Field<&Lantern::mShade>( "shade" );
    ComponentTypes componentTypes( reflection );
    componentTypes.Add<Lantern>();
    const ComponentTypes::Type& lantern = *componentTypes.TryFind( "lantern"_n );

    Scene scene;
    Fields fields;
    scene.ListenToFields( fields );
    const Entity hall = CreateChild( scene, scene.Root(), Make( "hall" ) );
    scene.Add<Lantern>( hall );
    const u32 h = static_cast<u32>( hall );

    REQUIRE( componentTypes.TrySetField( scene, hall, lantern, *FieldPath::TryParse( "glow" ), MetaAny( 3.0f ) ) );
    REQUIRE(
        componentTypes.TrySetField( scene, hall, lantern, *FieldPath::TryParse( "shade.softness" ), MetaAny( 0.5f ) ) );
    CHECK( scene.Get<Lantern>( hall ).mGlow == 3.0f );
    CHECK( scene.Get<Lantern>( hall ).mShade.mSoftness == 0.5f );
    CHECK( fields.mHeard == vector<string>{ format( "{} lantern.glow", h ), format( "{} lantern.shade", h ) } );

    // A set that fails tells no one.
    CHECK( componentTypes.TrySetField( scene, hall, lantern, *FieldPath::TryParse( "glow" ), MetaAny( Make( "x" ) ) )
               .error()
               .starts_with( "lantern.glow: " ) );
    scene.Remove<Lantern>( hall );
    CHECK(
        componentTypes.TrySetField( scene, hall, lantern, *FieldPath::TryParse( "glow" ), MetaAny( 1.0f ) ).error() ==
        "/hall has no lantern" );
    CHECK( fields.mHeard.size() == 2 );
}

TEST_CASE( "A component type is described first, added once, under its category" )
{
    Reflection reflection;
    ComponentTypes componentTypes( reflection );
    CHECK_THROWS_AS( componentTypes.Add<Lantern>(), logic_error );
    reflection.Type<Shade>( "shade" ).Field<&Shade::mSoftness>( "softness" );
    reflection.Type<Lantern>( "lantern" ).Field<&Lantern::mGlow>( "glow" ).Field<&Lantern::mShade>( "shade" );
    componentTypes.Add<Lantern>( "lighting" );
    CHECK_THROWS_AS( componentTypes.Add<Lantern>(), logic_error );
    CHECK( componentTypes.Names() == "lantern" );
    CHECK( componentTypes.All()[0].mCategory == "lighting" );
}
