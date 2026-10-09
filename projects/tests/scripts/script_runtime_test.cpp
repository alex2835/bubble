// Script files, their instances, coroutines, events and hot reload - the
// scripting model of the architecture doc, without a World yet.
#include "scripts/script_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

TEST_CASE( "A file's callbacks are found and self holds its props" )
{
    Scripts scripts;
    const auto player = scripts.Load( "player.luau", R"(
props {
    speed       = 40,
    jump_height = prop( 10, { min = 0, max = 30 } ),
    patrol      = { vector.create( 0, 0, 0 ) },
}
local TIME_TO_APEX = 0.5

function on_start( self )
    self.rise_gravity = 2 * self.jump_height / TIME_TO_APEX ^ 2
end

function on_hit( self )
    table.insert( self.patrol, vector.create( 1, 0, 0 ) )
end
)" );
    CHECK( scripts.mRuntime.Has( player, OnStart ) );
    CHECK_FALSE( scripts.mRuntime.Has( player, OnUpdate ) );

    const auto a = scripts.Make( player, "/a" );
    const auto b = scripts.Make( player, "/b" );
    REQUIRE( scripts.Call( a, OnStart ) );
    CHECK( scripts.Number( a, "rise_gravity" ) == 80 );
    CHECK( scripts.Number( a, "speed" ) == 40 );
    // A missing callback is not called and is no error.
    CHECK( scripts.Call( a, OnUpdate ) );

    // Each instance has its own copy of a table prop.
    REQUIRE( scripts.Call( a, OnHit ) );
    CHECK( LuaTable( scripts.Field( a, "patrol" ) ).Length() == 2 );
    CHECK( LuaTable( scripts.Field( b, "patrol" ) ).Length() == 1 );
}

TEST_CASE( "Overrides lay over the defaults and must match them" )
{
    Scripts scripts;
    const auto player = scripts.Load( "player.luau", "props { speed = 40, jump_height = 10 }" );

    auto fast = scripts.MakeWith( player, "{ speed = 80 }" );
    REQUIRE( fast );
    CHECK( scripts.Number( *fast, "speed" ) == 80 );
    CHECK( scripts.Number( *fast, "jump_height" ) == 10 );

    auto misspelt = scripts.MakeWith( player, "{ speeed = 80 }" );
    REQUIRE_FALSE( misspelt );
    CHECK( misspelt.error() == "/player: \"speeed\" is not a prop of player.luau (props: jump_height, speed)" );

    auto wrongType = scripts.MakeWith( player, "{ speed = 'fast' }" );
    REQUIRE_FALSE( wrongType );
    CHECK( wrongType.error() == "/player: prop speed is a number in player.luau, the override is a string" );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "Locals are each instance's own state, which the scene cannot set" )
{
    Scripts scripts;
    const auto gun = scripts.Load( "gun.luau", R"(
props { reload_time = 2 }
locals { l_reload = 0, l_shots = {} }

function on_hit( self )
    self.l_reload = self.reload_time
    table.insert( self.l_shots, self.l_reload )
end
)" );
    const auto a = scripts.Make( gun, "/a" );
    const auto b = scripts.Make( gun, "/b" );
    REQUIRE( scripts.Call( a, OnHit ) );
    CHECK( scripts.Number( a, "l_reload" ) == 2 );
    CHECK( scripts.Number( b, "l_reload" ) == 0 );
    CHECK( LuaTable( scripts.Field( a, "l_shots" ) ).Length() == 1 );
    CHECK( LuaTable( scripts.Field( b, "l_shots" ) ).Length() == 0 );
    // Not a setting: the props leave it out and the scene cannot set it.
    CHECK( scripts.mRuntime.Props( gun ).RawGet( "l_reload" ).IsNil() );
    auto overridden = scripts.MakeWith( gun, "{ l_reload = 5 }" );
    REQUIRE_FALSE( overridden );
    CHECK( overridden.error() ==
           "/player: \"l_reload\" is a local of gun.luau: it starts the same in every instance, the scene cannot "
           "set it" );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "locals is declared once, with names of its own" )
{
    Scripts scripts;
    CHECK( scripts.LoadError( "a.luau", "locals { a = 1 }\nlocals { b = 2 }" )
               .find( "locals is declared once, at the top of the file" ) != string::npos );
    CHECK( scripts.LoadError( "b.luau", "props { speed = 1 }\nlocals { speed = 0 }" )
               .find( "b.luau: speed is both a prop and a local; self has one of each name" ) != string::npos );
    CHECK( scripts.LoadError( "c.luau", "locals { entity = 1 }" ).find( "a local cannot be called 'entity'" ) !=
           string::npos );
}

TEST_CASE( "Props that a scene file could not hold are refused on load" )
{
    Scripts scripts;
    CHECK( scripts.LoadError( "a.luau", "props { entity = 1 }" ).find( "cannot be called 'entity'" ) != string::npos );
    CHECK( scripts.LoadError( "b.luau", "props { hit = function() end }" ).find( "a function cannot be saved" ) !=
           string::npos );
    CHECK( scripts.LoadError( "c.luau", "props { a = 1 }\nprops { b = 2 }" ).find( "declared once" ) != string::npos );
    CHECK( scripts.LoadError( "d.luau", "on_update = 5" ).find( "on_update is a number, not a function" ) !=
           string::npos );
}

TEST_CASE( "A file's globals are declared at its top; a new one later is an error" )
{
    Scripts scripts;
    LogWatch log;
    const auto counter = scripts.Load( "counter.luau", R"(
count = 0
function on_update( self, dt )
    count = count + 1
    self.count = count
end
function on_hit( self )
    score = 1
end
)" );
    const auto a = scripts.Make( counter );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK( scripts.Number( a, "count" ) == 2 );

    CHECK_FALSE( scripts.Call( a, OnHit ) );
    CHECK_FALSE( scripts.mRuntime.Enabled( a ) );
    CHECK( log.Saw( LogLevel::Error, { "/player", "undeclared global 'score'" } ) );
}

TEST_CASE( "Each instance keeps its own state in self" )
{
    Scripts scripts;
    const auto gun = scripts.Load( "gun.luau", R"(
props { reload_time = 2 }

function on_start( self )
    self.reload_progress = 0
end

function on_update( self, dt )
    if self.reload_progress > 0 then
        self.reload_progress -= dt
    end
end

function on_hit( self )
    self.reload_progress = self.reload_time
end
)" );
    const auto a = scripts.Make( gun, "/a" );
    const auto b = scripts.Make( gun, "/b" );
    for ( const auto handle : { a, b } )
        REQUIRE( scripts.Call( handle, OnStart ) );

    REQUIRE( scripts.Call( a, OnHit ) );
    for ( const auto handle : { a, b } )
        REQUIRE( scripts.mRuntime.Call( handle, OnUpdate, 0.5 ) );

    CHECK( scripts.Number( a, "reload_progress" ) == 1.5 );
    // Never hit: its reload never started.
    CHECK( scripts.Number( b, "reload_progress" ) == 0 );
}

TEST_CASE( "A failing instance is logged and switched off; the others go on" )
{
    Scripts scripts;
    LogWatch log;
    const auto unit = scripts.Load( "unit.luau", R"(
props { fragile = false }
function on_update( self, dt )
    if self.fragile then
        error( "broke" )
    end
    self.ticks = ( self.ticks or 0 ) + 1
end
)" );
    auto weak = scripts.MakeWith( unit, "{ fragile = true }", "/weak" );
    REQUIRE( weak );
    const auto strong = scripts.Make( unit, "/strong" );

    CHECK_FALSE( scripts.mRuntime.Call( *weak, OnUpdate, 0.016 ) );
    CHECK( scripts.mRuntime.Call( strong, OnUpdate, 0.016 ) );
    CHECK( lua_gettop( scripts.L() ) == 0 );

    CHECK_FALSE( scripts.mRuntime.Enabled( *weak ) );
    CHECK( scripts.mRuntime.Enabled( strong ) );
    CHECK( log.Saw( LogLevel::Error, { "/weak", "unit.luau:5: broke", "on_update" } ) );
    // Switched off: no call, no error.
    CHECK( scripts.mRuntime.Call( *weak, OnUpdate, 0.016 ) );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "A destroyed instance's handle finds nothing, even once its slot is reused" )
{
    Scripts scripts;
    const auto unit = scripts.Load( "unit.luau", "props { hp = 3 }" );
    const auto first = scripts.Make( unit, "/first" );
    scripts.mRuntime.Destroy( first );
    // The freed slot goes to the next instance; the old handle stays dead.
    const auto second = scripts.Make( unit, "/second" );
    CHECK( second.mIndex == first.mIndex );
    CHECK_FALSE( scripts.mRuntime.Alive( first ) );
    CHECK( scripts.mRuntime.Alive( second ) );

    // A call through it does nothing and says so; nothing is logged, the
    // entity is just gone.
    LogWatch log;
    CHECK_FALSE( scripts.mRuntime.Call( first, OnUpdate, 1.0 ) );
    CHECK( log.Entries().empty() );
    CHECK( lua_gettop( scripts.L() ) == 0 );
    CHECK( scripts.mRuntime.Self( first ).IsNil() );
    // Destroying twice is harmless.
    scripts.mRuntime.Destroy( first );
    CHECK( scripts.mRuntime.Alive( second ) );
}

TEST_CASE( "An instance can be destroyed from inside its own callback" )
{
    Scripts scripts;
    const auto unit = scripts.Load( "unit.luau", R"(
function on_start( self )
    on( "ping", function( self ) record( "pong" ) end )
    start( function()
        wait( 1 )
        record( "woke up" )
    end )
end
function on_hit( self )
    destroy()
    -- self and the running function stay until it returns.
    self.after = true
    record( "still running" )
end
)" );
    scripts.mDoomed = scripts.Make( unit );
    REQUIRE( scripts.Call( scripts.mDoomed, OnStart ) );
    REQUIRE( scripts.Call( scripts.mDoomed, OnHit ) );
    CHECK( scripts.mRecorded == vector<string>{ "still running" } );
    CHECK_FALSE( scripts.mRuntime.Alive( scripts.mDoomed ) );

    // Its subscription and coroutine went with it.
    scripts.mRuntime.Emit( "ping" );
    scripts.mRuntime.Tick( 2 );
    CHECK( scripts.mRecorded == vector<string>{ "still running" } );
}

TEST_CASE( "A misspelt callback is pointed out" )
{
    Scripts scripts;
    LogWatch log;
    scripts.Load( "typo.luau", "function on_updat( self, dt ) end" );
    CHECK( log.Saw( LogLevel::Warning, { "typo.luau", "on_updat is not a callback", "on_update" } ) );
}

TEST_CASE( "A changed file swaps the functions of every instance and keeps self" )
{
    Scripts scripts;
    const auto door = scripts.Load( "door.luau", R"(
props { speed = 1 }
locals { l_open = 0 }
function on_update( self, dt )
    self.version = 1
    self.calls = ( self.calls or 0 ) + 1
    self.l_open = 7
end
function on_hit( self )
    error( "not yet" )
end
)" );
    const auto a = scripts.Make( door );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK_FALSE( scripts.Call( a, OnHit ) );
    CHECK_FALSE( scripts.mRuntime.Enabled( a ) );

    REQUIRE( scripts.Change( "door.luau", R"(
props { speed = 1, armor = 5 }
locals { l_open = 0, l_lock = 3 }
function on_update( self, dt )
    self.version = 2
    self.calls = ( self.calls or 0 ) + 1
end
)" ) );
    CHECK( scripts.mRuntime.Enabled( a ) );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK( scripts.Number( a, "version" ) == 2 );
    CHECK( scripts.Number( a, "calls" ) == 2 );
    CHECK( scripts.Number( a, "armor" ) == 5 );
    // A local keeps what the instance made of it; a new one starts.
    CHECK( scripts.Number( a, "l_open" ) == 7 );
    CHECK( scripts.Number( a, "l_lock" ) == 3 );
    CHECK_FALSE( scripts.mRuntime.Has( door, OnHit ) );

    // A file that fails as it runs leaves the old one working, and says so.
    LogWatch log;
    REQUIRE( scripts.Change( "door.luau", "error( 'half-saved' )" ) );
    CHECK( log.Saw( LogLevel::Error, { "door.luau:1: half-saved" } ) );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK( scripts.Number( a, "calls" ) == 3 );

    // One that does not compile never reaches the runtime: the registry
    // keeps the version it had.
    CHECK_FALSE( scripts.Change( "door.luau", "function on_update( self" ) );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK( scripts.Number( a, "calls" ) == 4 );
}

TEST_CASE( "Unloading a file destroys its instances" )
{
    Scripts scripts;
    const auto door = scripts.Load( "door.luau", "props { open = false }" );
    const auto a = scripts.Make( door );
    scripts.mRuntime.Unload( door );
    CHECK_FALSE( scripts.mRuntime.Alive( a ) );
    CHECK_FALSE( scripts.mRuntime.Create( door, "/late" ).has_value() );
    // Loading it again runs it anew, as a new script.
    const auto again = scripts.mRuntime.Load( "door.luau" );
    REQUIRE( again );
    CHECK( *again != door );
}

TEST_CASE( "The runtime runs what the registry holds, each file once" )
{
    Scripts scripts;
    const auto door = scripts.Load( "door.luau", "record( 'ran' )" );
    // The same path is the same script; the file does not run again.
    CHECK( scripts.mRuntime.Load( "door.luau" ) == door );
    CHECK( scripts.mRecorded == vector<string>{ "ran" } );

    // A file the registry has not loaded is an error, not a read from disk.
    scripts.mFiles["late.luau"] = "props {}";
    auto late = scripts.mRuntime.Load( "late.luau" );
    REQUIRE_FALSE( late );
    CHECK( late.error().mMessage == "late.luau is not loaded" );
}

TEST_CASE( "Coroutines wait for time and for conditions" )
{
    Scripts scripts;
    const auto cutscene = scripts.Load( "cutscene.luau", R"(
function on_start( self )
    self.step = 0
    start( function( first )
        self.step = first
        wait( 1 )
        self.step = 2
        wait_until( function() return self.go end )
        self.step = 3
    end, 1 )
end
)" );
    const auto a = scripts.Make( cutscene );
    REQUIRE( scripts.Call( a, OnStart ) );
    CHECK( scripts.Number( a, "step" ) == 1 );
    scripts.mRuntime.Tick( 0.5f );
    CHECK( scripts.Number( a, "step" ) == 1 );
    scripts.mRuntime.Tick( 0.6f );
    CHECK( scripts.Number( a, "step" ) == 2 );
    scripts.mRuntime.Tick( 1 );
    CHECK( scripts.Number( a, "step" ) == 2 );
    scripts.Set( a, "go", true );
    scripts.mRuntime.Tick( 0 );
    CHECK( scripts.Number( a, "step" ) == 3 );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "A coroutine's error switches its instance off; waiting outside one is an error" )
{
    Scripts scripts;
    LogWatch log;
    const auto bomb = scripts.Load( "bomb.luau", R"(
function on_start( self )
    start( function()
        wait( 0 )
        error( "boom" )
    end )
end
function on_update( self, dt )
    wait( 1 )
end
)" );
    const auto a = scripts.Make( bomb, "/bomb" );
    REQUIRE( scripts.Call( a, OnStart ) );
    scripts.mRuntime.Tick( 0.016f );
    CHECK_FALSE( scripts.mRuntime.Enabled( a ) );
    CHECK( log.Saw( LogLevel::Error, { "/bomb", "boom" } ) );

    const auto b = scripts.Make( bomb );
    CHECK_FALSE( scripts.Call( b, OnUpdate ) );
    CHECK( log.Saw( LogLevel::Error, { "wait works inside start" } ) );

    CHECK( scripts.LoadError( "top.luau", "start( function() end )" ).find( "in an entity's callbacks" ) !=
           string::npos );
}

TEST_CASE( "Events reach subscribers in order and leave with them" )
{
    Scripts scripts;
    const auto listener = scripts.Load( "listener.luau", R"(
props { name = "" }
function on_start( self )
    on( "door_opened", function( self, door )
        if self.name == "grumpy" then error( "not interested" ) end
        record( self.name .. ":" .. door )
    end )
end
)" );
    const auto opener = scripts.Load( "opener.luau", R"(
function on_hit( self )
    emit( "door_opened", "north" )
end
)" );
    auto a = scripts.MakeWith( listener, "{ name = 'a' }", "/a" );
    auto grumpy = scripts.MakeWith( listener, "{ name = 'grumpy' }", "/grumpy" );
    auto b = scripts.MakeWith( listener, "{ name = 'b' }", "/b" );
    auto c = scripts.MakeWith( listener, "{ name = 'c' }", "/c" );
    REQUIRE( ( a and grumpy and b and c ) );
    for ( const ScriptInstanceHandle instance : { *a, *grumpy, *b, *c } )
        REQUIRE( scripts.Call( instance, OnStart ) );

    const auto door = scripts.Make( opener, "/door" );
    REQUIRE( scripts.Call( door, OnHit ) );
    CHECK( scripts.mRecorded == vector<string>{ "a:north", "b:north", "c:north" } );
    CHECK_FALSE( scripts.mRuntime.Enabled( *grumpy ) );

    // b leaves, and a newcomer takes its slot without inheriting its
    // subscription; C++ emits too.
    scripts.mRuntime.Destroy( *b );
    auto newcomer = scripts.MakeWith( listener, "{ name = 'new' }", "/new" );
    REQUIRE( newcomer );
    CHECK( newcomer->mIndex == b->mIndex );
    scripts.mRecorded.clear();
    scripts.mRuntime.Emit( "door_opened", "south" );
    CHECK( scripts.mRecorded == vector<string>{ "a:south", "c:south" } );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "A --!native file runs the same as interpreted" )
{
    Scripts scripts;
    const auto math = scripts.Load( "math.luau", R"(--!native
function on_update( self, dt )
    local sum = vector.zero
    for i = 1, 1000 do
        sum += vector.create( i, i * 2, 1 ) * 0.5
    end
    self.total = sum.x + sum.y + sum.z
end
)" );
    const auto a = scripts.Make( math );
    REQUIRE( scripts.Call( a, OnUpdate ) );
    CHECK( scripts.Number( a, "total" ) == doctest::Approx( 0.5 * ( 500500 + 1001000 + 1000 ) ) );
#ifdef BUBBLE_LUAU_CODEGEN
    // Desktop CPUs are all ones Luau makes code for; the web has no codegen.
    CHECK( scripts.mState.NativeCode() );
#endif
}
