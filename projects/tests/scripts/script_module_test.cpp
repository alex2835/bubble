// A world's scripts module: an instance for each entity's Script, started
// once its file is in, called in the frame's order, its props from the
// scene, self kept by a snapshot, and the engine's types in its Luau.
#include "bubble/scripts/script_values.hpp"
#include "bubble/types/algorithm.hpp"
#include "scripts/script_world.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

namespace
{
constexpr string_view cLifecycle = R"(
function on_start( self )
    record( "start " .. self.entity.name )
end

function on_fixed_update( self, dt )
    record( string.format( "fixed %.4f", dt ) )
end

function on_update( self, dt )
    record( "update" )
end

function on_destroy( self )
    record( "destroy " .. tostring( self.entity.alive ) )
end
)";
}

TEST_CASE( "An entity's script starts once its file is in, then runs each fixed step and each frame" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/life.luau", cLifecycle );
    Scene scene;
    const Entity player = ScriptWorld::Add( scene, "player", script );
    World world( w.mApp, std::move( scene ) );

    w.Load( world );
    // Started at the first frame the file was in, on_update in that frame.
    const vector<string> loaded = w.TakeRecorded();
    REQUIRE_FALSE( loaded.empty() );
    CHECK( loaded.front() == "start player" );
    CHECK( ranges::count( loaded, "start player"s ) == 1 );
    CHECK( ranges::count( loaded, "update"s ) == static_cast<ptrdiff_t>( loaded.size() - 1 ) );

    world.Step();
    world.Frame( 0.0 );
    CHECK( w.TakeRecorded() == vector<string>{ "fixed 0.0167", "update" } );
    // Paused, the frame's own phase still runs: a menu works.
    world.SetPaused( true );
    world.Frame( 1.0 );
    CHECK( w.TakeRecorded() == vector<string>{ "update" } );
    CHECK( world.GetScene().Get<ScriptInstance>( player ).mInstanceHandle );
}

TEST_CASE( "A script removed, or its entity destroyed, hears on_destroy - the entity gone by then when destroyed" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/life.luau", cLifecycle );
    Scene scene;
    const Entity kept = ScriptWorld::Add( scene, "kept", script );
    const Entity doomed = ScriptWorld::Add( scene, "doomed", script );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    (void)w.TakeRecorded();

    world.GetScene().Remove<Script>( kept );
    world.GetScene().Destroy( doomed );
    world.Frame( 0.0 );
    CHECK( w.TakeRecorded() == vector<string>{ "destroy true", "destroy false" } );
    CHECK_FALSE( world.GetScene().Has<ScriptInstance>( kept ) );
    world.Frame( 0.0 );
    CHECK( w.TakeRecorded().empty() );
}

TEST_CASE( "The scene's overrides are laid on the file's props - colors, assets and links to entities too" )
{
    ScriptWorld w;
    const AssetId model = w.Write( "scripts/model_stand_in.luau", "" );
    const AssetId script = w.Write( "scripts/mover.luau", R"(
props {
    speed = 4,
    tint = color( 1, 0, 0 ),
    target = entity(),
    model = asset(),
    stops = { vector.create( 0, 0, 0 ) },
}

function on_start( self )
    record( string.format( "%g %s %s %s %d", self.speed, self.target.path, tostring( self.tint ), self.model.path,
                           #self.stops ) )
end
)" );
    Scene scene;
    const Entity door = CreateChild( scene, scene.Root(), MakeText( "door" ) );
    ScriptWorld::Add( scene, "mover", script,
                      { Prop( "speed", Value( 7.0 ) ), Prop( "target", Value( EntityPath{ MakeText( "../door" ) } ) ),
                        Prop( "tint", Value( Color{ .mR = 0, .mG = 1, .mB = 0, .mA = 1 } ) ),
                        Prop( "model", Value( model ) ),
                        Prop( "stops", Value( Value::List{ Value( vec3( 1.0f ) ), Value( vec3( 2.0f ) ) } ) ) } );
    ScriptWorld::Add( scene, "plain", script );
    World world( w.mApp, std::move( scene ) );
    (void)door;

    const LogWatch log;
    w.Load( world );
    // The plain one's target is the empty entity: reading its path is an
    // error, logged, and it is switched off.
    const vector<string> recorded = w.TakeRecorded();
    CHECK( recorded == vector<string>{ "7 /door color( 0, 1, 0, 1 ) scripts/model_stand_in.luau 2" } );
    CHECK( log.Saw( LogLevel::Error, { "/plain", "mover.luau:11:", "an empty entity" } ) );
}

TEST_CASE( "An override the file does not have, or of another kind, fails that entity and says why" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/mover.luau", R"(
props { speed = 4, tint = color( 1, 0, 0 ) }
locals { ticks = 0 }
function on_start( self ) record( "start" ) end
)" );
    Scene scene;
    ScriptWorld::Add( scene, "typo", script, { Prop( "sped", Value( 7.0 ) ) } );
    ScriptWorld::Add( scene, "kind", script, { Prop( "tint", Value( 7.0 ) ) } );
    ScriptWorld::Add( scene, "local", script, { Prop( "ticks", Value( 7.0 ) ) } );
    ScriptWorld::Add( scene, "lost", script, { Prop( "speed", Value( EntityPath{ MakeText( "/nowhere" ) } ) ) } );
    World world( w.mApp, std::move( scene ) );

    const LogWatch log;
    w.Load( world );
    CHECK( w.TakeRecorded().empty() );
    CHECK( log.Saw( LogLevel::Error, { "/typo: \"sped\" is not a prop of scripts/mover.luau (props: speed, tint)" } ) );
    CHECK(
        log.Saw( LogLevel::Error, { "/kind: prop tint is a color in scripts/mover.luau, the override is a number" } ) );
    CHECK( log.Saw( LogLevel::Error, { "/local: \"ticks\" is a local of scripts/mover.luau" } ) );
    CHECK( log.Saw( LogLevel::Error, { "/lost: script.props.speed: /nowhere" } ) );
}

TEST_CASE( "A prop the scene sets while the game runs is laid on self, and the rest of self stays" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/counter.luau", R"(
props { speed = 4 }
locals { ticks = 0 }
function on_fixed_update( self, dt )
    self.ticks += 1
    record( string.format( "%g %d", self.speed, self.ticks ) )
end
)" );
    Scene scene;
    const Entity counter = ScriptWorld::Add( scene, "counter", script );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    world.Step();
    w.SetProps( world, counter, { Prop( "speed", Value( 9.0 ) ) } );
    world.Step();
    // Back to the file's default when the override goes.
    w.SetProps( world, counter, {} );
    world.Step();
    CHECK( w.TakeRecorded() == vector<string>{ "4 1", "9 2", "4 3" } );

    const LogWatch log;
    w.SetProps( world, counter, { Prop( "speed", Value( MakeText( "fast" ) ) ) } );
    world.Step();
    CHECK( log.Saw( LogLevel::Error, { "/counter: prop speed is a number in scripts/counter.luau" } ) );
    CHECK( w.TakeRecorded() == vector<string>{ "4 4" } );
}

TEST_CASE( "A script that does not load is logged once, and loads when a script changes" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/broken.luau", "error( 'not yet' )" );
    Scene scene;
    const Entity entity = ScriptWorld::Add( scene, "broken", script );
    World world( w.mApp, std::move( scene ) );

    const LogWatch log;
    w.Load( world );
    CHECK( log.Saw( LogLevel::Error, { "/broken", "broken.luau:1: not yet" } ) );
    CHECK( log.Entries().size() == 1 );
    CHECK( world.GetScene().Get<ScriptInstance>( entity ).mFailed );

    w.Change( "scripts/broken.luau", "function on_start( self ) record( 'fixed' ) end" );
    w.Load( world );
    CHECK( w.TakeRecorded() == vector<string>{ "fixed" } );
}

TEST_CASE( "Each step's scripts go in the order their entities' scripts lie, the same after a snapshot" )
{
    ScriptWorld w;
    const AssetId script =
        w.Write( "scripts/name.luau", "function on_fixed_update( self, dt ) record( self.entity.name ) end" );
    Scene scene;
    for ( const string_view name : { "a", "b", "c", "d" } )
        ScriptWorld::Add( scene, name, script );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    // A removal moves the last into the hole: d before c from here on.
    world.GetScene().Remove<Script>( *TryResolve( world.GetScene(), EntityPath{ MakeText( "/b" ) } ) );
    world.Step();
    const vector<string> order = w.TakeRecorded();
    CHECK( order.size() == 3 );

    const WorldSnapshot snapshot = world.Snapshot();
    world.Restore( snapshot );
    w.Load( world );
    (void)w.TakeRecorded();
    world.Step();
    CHECK( w.TakeRecorded() == order );
}

TEST_CASE( "A snapshot keeps self: restored here it comes back and starts again, in another world it starts anew" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/counter.luau", R"(
locals { ticks = 0 }
function on_start( self ) record( "start " .. self.ticks ) end
function on_fixed_update( self, dt ) self.ticks += 1 end
function on_destroy( self ) record( "destroy" ) end
)" );
    Scene scene;
    ScriptWorld::Add( scene, "counter", script );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    for ( int step = 0; step < 3; ++step )
        world.Step();
    const WorldSnapshot snapshot = world.Snapshot();
    for ( int step = 0; step < 2; ++step )
        world.Step();
    CHECK( w.TakeRecorded() == vector<string>{ "start 0" } );

    // Twice: a snapshot restores as often as it is asked. No on_destroy -
    // nothing in the game ended.
    for ( int again = 0; again < 2; ++again )
    {
        world.Restore( snapshot );
        w.Load( world );
        CHECK( w.TakeRecorded() == vector<string>{ "start 3" } );
        world.Step();
    }

    World other( w.mApp, Scene() );
    other.Restore( snapshot );
    w.Load( other );
    CHECK( w.TakeRecorded() == vector<string>{ "start 0" } );
}

TEST_CASE( "A snapshot outlives its world without holding on to its Luau" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/keep.luau", "locals { bag = { 1, 2, 3 } }" );
    Scene scene;
    ScriptWorld::Add( scene, "keeper", script );
    opt<WorldSnapshot> kept;
    {
        World world( w.mApp, std::move( scene ) );
        w.Load( world );
        kept = world.Snapshot();
    }
    World later( w.mApp, Scene() );
    later.Restore( *kept );
    w.Load( later );
    kept.reset();
    const Entity keeper = *TryResolve( later.GetScene(), EntityPath{ MakeText( "/keeper" ) } );
    CHECK( w.Self( later, keeper )["bag"].Kind() == LuaKind::Table );
}

TEST_CASE( "An entity in Luau: its name, path and parent, children found, compared and destroyed" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/look.luau", R"(
function on_start( self )
    local e = self.entity
    local door = e:find( "door" )
    local same = e:child( "door" )
    record( e.name .. " " .. e.path .. " " .. tostring( e.parent ) )
    record( door.path .. " " .. tostring( door == same ) .. " " .. tostring( door == e ) )
    record( tostring( e:child( "window" ) ) .. " " .. tostring( e:find( "/house/door" ) == door ) )
    door:destroy()
    record( tostring( door.alive ) .. " " .. tostring( door ) )
    record( typeof( e ) )
end
)" );
    Scene scene;
    const Entity house = ScriptWorld::Add( scene, "house", script );
    CreateChild( scene, house, MakeText( "door" ) );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    CHECK( w.TakeRecorded() == vector<string>{ "house /house /", "/house/door true false", "nil true",
                                               "false entity 2 (gone)", "entity" } );
}

TEST_CASE( "math.random draws on the world's Random: the same seed draws the same, and a snapshot keeps it" )
{
    ScriptWorld w;
    const AssetId script = w.Write( "scripts/dice.luau", R"(
function on_fixed_update( self, dt )
    record( string.format( "%d %d %.3f", math.random( 6 ), math.random( 10, 20 ), math.random() ) )
end
)" );
    const auto play = [&]( u64 seed ) {
        Scene scene;
        ScriptWorld::Add( scene, "dice", script );
        World world( w.mApp, std::move( scene ), WorldOptions{ .mStepsPerSecond = 60, .mSeed = seed } );
        w.Load( world );
        for ( int step = 0; step < 4; ++step )
            world.Step();
        return w.TakeRecorded();
    };
    const vector<string> first = play( 7 );
    CHECK( first.size() == 4 );
    CHECK( play( 7 ) == first );
    CHECK( play( 8 ) != first );

    Scene scene;
    ScriptWorld::Add( scene, "dice", script );
    World world( w.mApp, std::move( scene ) );
    w.Load( world );
    const WorldSnapshot snapshot = world.Snapshot();
    world.Step();
    const vector<string> drawn = w.TakeRecorded();
    world.Restore( snapshot );
    w.Load( world );
    world.Step();
    CHECK( w.TakeRecorded() == drawn );

    const LogWatch log;
    w.Change( "scripts/dice.luau", "function on_fixed_update( self, dt ) math.randomseed( 1 ) end" );
    world.Step();
    CHECK( log.Saw( LogLevel::Error, { "a world's numbers come from its seed" } ) );
}

TEST_CASE( "A value of every kind becomes Luau and comes back as it was" )
{
    ScriptWorld w;
    Scene scene;
    const Entity door = CreateChild( scene, scene.Root(), MakeText( "door" ) );
    World world( w.mApp, std::move( scene ) );
    LuaState& lua = world.Get<ScriptRuntime>().Lua();
    const AssetId model = w.Write( "scripts/stand_in.luau", "" );

    const Value value( Value::Record{ Prop( "count", Value( 3.0 ) ), Prop( "flag", Value( true ) ),
                                      Prop( "label", Value( MakeText( "hi" ) ) ),
                                      Prop( "where", Value( vec3( 1.0f, 2.0f, 3.0f ) ) ),
                                      Prop( "tint", Value( Color{ .mR = 0.5f, .mG = 0, .mB = 1, .mA = 1 } ) ),
                                      Prop( "kind", Value( Name::Declare( "jump" ) ) ), Prop( "model", Value( model ) ),
                                      Prop( "door", Value( EntityPath{ MakeText( "/door" ) } ) ),
                                      Prop( "steps", Value( Value::List{ Value( 1.0 ), Value( 2.0 ) } ) ) } );
    const ValueType type = ValueType::RecordOf( {
        ValueTypeField{ "count"_n, ValueType::Of( ValueKind::Integer ) },
        ValueTypeField{ "flag"_n, ValueType::Of( ValueKind::Bool ) },
        ValueTypeField{ "label"_n, ValueType::Of( ValueKind::Text ) },
        ValueTypeField{ "where"_n, ValueType::Of( ValueKind::Vec3 ) },
        ValueTypeField{ "tint"_n, ValueType::Of( ValueKind::Color ) },
        ValueTypeField{ "kind"_n, ValueType::Of( ValueKind::Name ) },
        ValueTypeField{ "model"_n, ValueType::Of( ValueKind::Asset ) },
        ValueTypeField{ "door"_n, ValueType::Of( ValueKind::Entity ) },
        ValueTypeField{ "steps"_n, ValueType::ListOf( ValueType::Of( ValueKind::Number ) ) },
    } );
    const auto luaMaybe = TryLuaFromValue( lua, value, world.GetScene(), world.GetScene().Root(), "props" );
    REQUIRE_MESSAGE( luaMaybe, luaMaybe.error() );
    CHECK( LuaTable( *luaMaybe )["door"].TryAs<Entity>() == door );
    const auto backMaybe = TryValueFromLua( lua, *luaMaybe, type, world.GetScene(), "props" );
    REQUIRE_MESSAGE( backMaybe, backMaybe.error() );
    CHECK( *backMaybe == value );
    // Untyped, a table reads back by its kinds; a whole number expected is
    // checked.
    CHECK( TryValueFromLua( lua, *luaMaybe, ValueType::Of( ValueKind::Any ), world.GetScene(), "props" ) );
    CHECK( TryValueFromLua( lua, lua.Value( 2.5 ), ValueType::Of( ValueKind::Integer ), world.GetScene(), "count" )
               .error() == "count: an integer expected, got 2.5" );
    CHECK(
        TryValueFromLua( lua, lua.Value( 2.5 ), ValueType::Of( ValueKind::Color ), world.GetScene(), "tint" ).error() ==
        "tint: a color expected, got 2.5" );
}

TEST_CASE( "A script's libraries, named by its requires, are loaded before it runs, all the way down" )
{
    ScriptWorld w;
    w.Write( "scripts/lib/inner.luau", "return { value = 5 }" );
    w.Write( "scripts/lib/outer.luau", "local inner = require( './inner' )\nreturn { value = inner.value * 2 }" );
    const AssetId script = w.Write( "scripts/user.luau", R"(
local outer = require( "./lib/outer" )
function on_start( self ) record( tostring( outer.value ) ) end
)" );
    const AssetId lost = w.Write( "scripts/lost.luau", "local none = require( './lib/none' )" );
    Scene scene;
    ScriptWorld::Add( scene, "user", script );
    ScriptWorld::Add( scene, "lost", lost );
    World world( w.mApp, std::move( scene ) );

    const LogWatch log;
    w.Load( world, 8 );
    CHECK( w.TakeRecorded() == vector<string>{ "10" } );
    CHECK( log.Saw( LogLevel::Error, { "/lost: scripts/lib/none.luau" } ) );
}

TEST_CASE( "A script in an endless loop fails at the time limit, logged, and the world goes on" )
{
    ScriptWorld w( true, 0.02 );
    const AssetId stuck = w.Write( "scripts/stuck.luau", "function on_start( self ) while true do end end" );
    const AssetId fine = w.Write( "scripts/fine.luau", "function on_update( self, dt ) record( 'fine' ) end" );
    Scene scene;
    ScriptWorld::Add( scene, "stuck", stuck );
    ScriptWorld::Add( scene, "fine", fine );
    World world( w.mApp, std::move( scene ) );

    const LogWatch log;
    w.Load( world );
    CHECK( log.Saw( LogLevel::Error, { "/stuck: scripts/stuck.luau:1: ran longer than 20 ms - an endless loop?" } ) );
    CHECK_FALSE( w.TakeRecorded().empty() );
}
