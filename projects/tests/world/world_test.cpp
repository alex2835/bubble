// World: a scene, its modules, data events, the frame's order and the fixed
// step. The module here heats things: it builds a runtime from a heater's
// data, rebuilds it when the power changes, and warms what it heats each
// fixed step - with a little noise from the world's Random, so a replay that
// comes out the same shows the seed is the world's too.
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/world/world.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
struct Heater
{
    f32 mPower = 1;
};

struct Warmth
{
    f32 mDegrees = 0;
};

// Built from a Heater, never described, never saved.
struct HeaterRuntime
{
    f32 mPower = 0;
};

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

class Heating final : public Module
{
public:
    explicit Heating( World& world ) : mWorld( world )
    {
        world.OnAdded<Heater>( [this]( Entity entity ) { Build( entity ); } );
        world.OnChanged<Heater>( "power"_n, [this]( Entity entity ) { Build( entity ); } );
        world.OnRemoved<Heater>( [this]( Entity entity ) { mWorld.GetScene().Remove<HeaterRuntime>( entity ); } );
        world.AddStep( Phase::FixedUpdate, [this]( f32 dt ) { Heat( dt ); } );
        world.GetServices().Add( *this );
    }

    u32 Builds() const { return mBuilds; }

private:
    void Build( Entity entity )
    {
        Scene& scene = mWorld.GetScene();
        scene.Add<HeaterRuntime>( entity, scene.Get<Heater>( entity ).mPower );
        ++mBuilds;
    }

    void Heat( f32 dt )
    {
        mWorld.GetScene().ForEach<HeaterRuntime, Warmth>( [&]( Entity, HeaterRuntime& runtime, Warmth& warmth ) {
            warmth.mDegrees += runtime.mPower * dt + mWorld.GetRandom().Range( -0.01f, 0.01f );
        } );
    }

    World& mWorld;
    u32 mBuilds = 0;
};

ModuleType HeatingType()
{
    return ModuleType{
        .mName = "heating",
        .mDescribe =
            []( Reflection& reflection, ComponentTypes& componentTypes ) {
                reflection.Type<Heater>( "heater" ).Field<&Heater::mPower>( "power" );
                reflection.Type<Warmth>( "warmth" ).Field<&Warmth::mDegrees>( "degrees" );
                componentTypes.Add<Heater>();
                componentTypes.Add<Warmth>();
            },
        .mCreate = []( World& world ) -> Scope<Module> { return CreateScope<Heating>( world ); },
    };
}

struct Fixture
{
    App mApp;

    Fixture() { mApp.AddModule( HeatingType() ); }

    // A level with /stove: a heater that warms itself.
    Scene Level( f32 power = 2 )
    {
        Scene scene;
        const Entity stove = CreateChild( scene, scene.Root(), Make( "stove" ) );
        scene.Add<Heater>( stove, power );
        scene.Add<Warmth>( stove );
        return scene;
    }

    Entity Stove( World& world ) { return *TryResolve( world.GetScene(), EntityPath{ Make( "/stove" ) } ); }

    void SetPower( World& world, f32 power )
    {
        const ComponentTypes& componentTypes = mApp.GetComponentTypes();
        REQUIRE( componentTypes.TrySetField( world.GetScene(), Stove( world ), *componentTypes.TryFind( "heater"_n ),
                                             *FieldPath::TryParse( "power" ), MetaAny( power ) ) );
    }
};
}

TEST_CASE( "A module builds its runtime from data, hears a field change, and writes what comes of it" )
{
    Fixture f;
    World world( f.mApp, f.Level( 2 ) );
    Scene& scene = world.GetScene();
    const Entity stove = f.Stove( world );
    Heating& heating = world.Get<Heating>();

    // Built before the world's first step.
    CHECK( scene.Get<HeaterRuntime>( stove ).mPower == 2.0f );
    CHECK( heating.Builds() == 1 );

    world.Step();
    const f32 once = scene.Get<Warmth>( stove ).mDegrees;
    CHECK( once == doctest::Approx( 2.0f / 60 ).epsilon( 0.01 / ( 2.0 / 60 ) ) );

    // A change waits for the world's next turn, then rebuilds.
    f.SetPower( world, 6 );
    CHECK( scene.Get<HeaterRuntime>( stove ).mPower == 2.0f );
    world.Step();
    CHECK( scene.Get<HeaterRuntime>( stove ).mPower == 6.0f );
    CHECK( heating.Builds() == 2 );
    CHECK( scene.Get<Warmth>( stove ).mDegrees - once == doctest::Approx( 6.0f / 60 ).epsilon( 0.01 / ( 6.0 / 60 ) ) );

    // Data gone, runtime gone; the warmth stays where it got to.
    scene.Remove<Heater>( stove );
    world.Step();
    CHECK_FALSE( scene.Has<HeaterRuntime>( stove ) );
    const f32 last = scene.Get<Warmth>( stove ).mDegrees;
    world.Step();
    CHECK( scene.Get<Warmth>( stove ).mDegrees == last );
    CHECK( TrySaveScene( scene, f.mApp.GetComponentTypes() )->find( "runtime" ) == string::npos );
}

TEST_CASE( "Data events skip what is gone by their turn, and a component replaced whole is built again" )
{
    Fixture f;
    World world( f.mApp, Scene() );
    Scene& scene = world.GetScene();
    Heating& heating = world.Get<Heating>();
    CHECK( heating.Builds() == 0 );

    // Come and gone between two steps: nothing to build.
    const Entity brief = CreateChild( scene, scene.Root(), Make( "brief" ) );
    scene.Add<Heater>( brief );
    scene.Destroy( brief );
    world.Step();
    CHECK( heating.Builds() == 0 );

    const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
    scene.Add<Heater>( lamp, 1.0f );
    world.Step();
    scene.Add<Heater>( lamp, 4.0f );
    world.Frame( 0 );
    CHECK( heating.Builds() == 2 );
    CHECK( scene.Get<HeaterRuntime>( lamp ).mPower == 4.0f );
}

TEST_CASE( "A frame runs the steps its time makes due, then its own phases, in the world's one order" )
{
    App app;
    vector<string> ran;
    struct Ring
    {
    };
    app.AddModule( ModuleType{
        .mName = "recorder",
        .mDescribe = {},
        .mCreate = [&]( World& world ) -> Scope<Module> {
            const auto record = [&ran]( string what ) { return [&ran, what]( f32 ) { ran.push_back( what ); }; };
            // Added out of order: the world's list decides.
            world.AddStep( Phase::Render, record( "render" ) );
            world.AddStep( Phase::Update, record( "update" ) );
            world.AddStep( Phase::Physics, [&ran, &world]( f32 ) {
                ran.push_back( "physics" );
                world.GetEvents().Emit( Ring{} );
            } );
            world.AddStep( Phase::FixedUpdate, record( "fixed_update" ) );
            world.AddStep( Phase::Transforms, record( "transforms" ) );
            world.AddStep( Phase::Animation, record( "animation" ) );
            world.AddStep( Phase::Audio, record( "audio" ) );
            world.GetEvents().Subscribe<Ring>( [&ran]( const Ring& ) { ran.push_back( "event" ); } );
            return CreateScope<Module>();
        },
    } );
    World world( app, Scene() );

    world.Frame( 1.0 / 30 );
    CHECK( ran == vector<string>{ "fixed_update", "physics", "event", "fixed_update", "physics", "event", "update",
                                  "animation", "transforms", "audio", "render" } );
    CHECK( world.Steps() == 2 );

    // Paused: the frame's own phases go on, the steps stand - but "step"
    // takes exactly one.
    ran.clear();
    world.SetPaused( true );
    world.Frame( 1.0 / 30 );
    CHECK( ran == vector<string>{ "update", "animation", "transforms", "audio", "render" } );
    ran.clear();
    world.Step();
    CHECK( ran == vector<string>{ "fixed_update", "physics", "event" } );
    CHECK( world.Steps() == 3 );

    CHECK_THROWS_AS( world.Frame( -1 ), logic_error );
}

TEST_CASE( "A service is found by its type, and one no module added is a bug naming it" )
{
    Fixture f;
    World world( f.mApp, Scene() );
    CHECK( world.GetServices().TryGet<Heating>() );
    CHECK_FALSE( world.GetServices().TryGet<Fixture>() );
    CHECK_THROWS_AS( world.Get<Fixture>(), logic_error );
    CHECK_THROWS_AS( world.GetServices().Add( world.Get<Heating>() ), logic_error );
}

TEST_CASE( "Hooks are for described types and their fields, added when a module is made" )
{
    App app;
    app.AddModule( HeatingType() );
    World world( app, Scene() );
    struct Undescribed
    {
        f32 mValue = 0;
    };
    CHECK_THROWS_AS( world.OnAdded<Undescribed>( []( Entity ) {} ), logic_error );
    CHECK_THROWS_AS( world.OnChanged<Heater>( "warmth"_n, []( Entity ) {} ), logic_error );
    CHECK_THROWS_AS( app.AddModule( HeatingType() ), logic_error );
}

TEST_CASE( "The same level plays the same at every step, whatever the frame rate, and another seed does not" )
{
    Fixture f;
    // The level as a file would hold it, loaded into each world afresh.
    const string level = *TrySaveScene( f.Level( 3 ), f.mApp.GetComponentTypes() );

    // Every step's warmth, and the power turned up at step 20 - an input,
    // given at a step rather than at a time.
    const auto play = [&]( u64 seed, const function<f64()>& frameTime ) {
        App& app = f.mApp;
        World world( app, *TryLoadScene( level, app.GetComponentTypes() ), WorldOptions{ .mSeed = seed } );
        const Entity stove = f.Stove( world );
        vector<f32> warmth;
        world.AddStep( Phase::FixedUpdate, [&]( f32 ) {
            if ( world.Steps() == 20 )
                f.SetPower( world, 5 );
        } );
        world.AddStep( Phase::Physics,
                       [&]( f32 ) { warmth.push_back( world.GetScene().Get<Warmth>( stove ).mDegrees ); } );
        while ( world.Steps() < 60 )
            world.Frame( frameTime() );
        warmth.resize( 60 );
        return warmth;
    };

    const vector<f32> steady = play( 1, [] { return 1.0 / 60; } );
    Random jitter( 99 );
    const vector<f32> ragged = play( 1, [&] { return static_cast<f64>( jitter.Range( 0.001f, 0.1f ) ); } );
    const vector<f32> reseeded = play( 2, [] { return 1.0 / 60; } );
    CHECK( steady == ragged );
    CHECK( steady != reseeded );
    // The input took: the stove warms faster after step 20.
    CHECK( steady[40] - steady[39] > steady[10] - steady[9] );
}
