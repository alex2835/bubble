// Libraries: files with shared code that scripts require. Paths as Luau's
// own require reads them, one copy per world, nothing loaded from disk.
#include "scripts/script_fixture.hpp"

using namespace bubble;
using namespace bubble::test;

namespace
{
// A world whose project has scripts/lib behind @lib.
struct Project : Scripts
{
    Project() { REQUIRE( mRuntime.SetAlias( "lib", "scripts/lib" ) ); }
};

const char* const cInventory = R"(
local inventory = {}
inventory.added = 0

function inventory.add( items, kind, count )
    items[kind] = ( items[kind] or 0 ) + count
    inventory.added += count
end

return inventory
)";
}

TEST_CASE( "A script requires a library by alias and by relative path" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/inventory.luau", cInventory );
    scripts.AddLibrary( "scripts/player_tuning.luau", "return { coins_per_hit = 3 }" );
    const auto player = scripts.Load( "scripts/player.luau", R"(
local inventory = require( "@lib/inventory" )
local tuning = require( "./player_tuning" )

function on_start( self )
    self.items = {}
end

function on_hit( self )
    inventory.add( self.items, "coin", tuning.coins_per_hit )
    self.coins = self.items.coin
    self.added = inventory.added
end
)" );
    const auto a = scripts.Make( player, "/a" );
    const auto b = scripts.Make( player, "/b" );
    for ( const auto handle : { a, b } )
    {
        REQUIRE( scripts.Call( handle, OnStart ) );
        REQUIRE( scripts.Call( handle, OnHit ) );
    }
    // Each entity has its own items; the library's own state is the
    // world's, shared by both.
    CHECK( scripts.Number( a, "coins" ) == 3 );
    CHECK( scripts.Number( b, "coins" ) == 3 );
    CHECK( scripts.Number( b, "added" ) == 6 );
}

TEST_CASE( "A library runs once however many files require it" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/counter.luau", R"(
record( "ran" )
return {}
)" );
    scripts.AddLibrary( "scripts/lib/a.luau", "return { counter = require( './counter' ) }" );
    scripts.Load( "scripts/one.luau", "local counter = require( '@lib/counter' )" );
    scripts.Load( "scripts/two.luau", "local a = require( '@lib/a' )" );
    CHECK( scripts.mRecorded == vector<string>{ "ran" } );
}

TEST_CASE( "require says what is wrong with a path" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/inventory.luau", cInventory );
    CHECK( scripts.LoadError( "scripts/a.luau", "require( 'scripts/lib/inventory' )" )
               .find( "require( 'scripts/lib/inventory' ): a path starts with ./, ../ or @alias" ) !=
           string::npos );
    CHECK( scripts.LoadError( "scripts/b.luau", "require( '@libs/inventory' )" )
               .find( "no alias @libs (aliases: @lib)" ) != string::npos );
    CHECK( scripts.LoadError( "scripts/c.luau", "require( '@lib/inventroy' )" )
               .find( "no library scripts/lib/inventroy.luau is loaded" ) != string::npos );
    CHECK( scripts.LoadError( "scripts/d.luau", "require( '../../outside' )" )
               .find( "the path leads out of the project" ) != string::npos );
    // The error points at the require line of the file.
    CHECK( scripts.LoadError( "scripts/e.luau", "\nrequire( '@lib/nothing' )" ).find( "scripts/e.luau:2:" ) !=
           string::npos );
}

TEST_CASE( "../ climbs from the requiring file's folder" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/shared/math.luau", "return { two = 2 }" );
    scripts.AddLibrary( "scripts/lib/doubler.luau", R"(
local math2 = require( "../shared/math" )
return { double = function( x ) return x * math2.two end }
)" );
    const auto unit = scripts.Load( "scripts/units/unit.luau", R"(
local doubler = require( "@lib/doubler" )
function on_start( self ) self.value = doubler.double( 21 ) end
)" );
    const auto a = scripts.Make( unit );
    REQUIRE( scripts.Call( a, OnStart ) );
    CHECK( scripts.Number( a, "value" ) == 42 );
}

TEST_CASE( "A library returns what it shares and has no props or callbacks" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/silent.luau", "local x = 1" );
    scripts.AddLibrary( "scripts/lib/with_props.luau", "props { speed = 1 }\nreturn {}" );
    scripts.AddLibrary( "scripts/lib/with_callback.luau", "function on_update( self, dt ) end\nreturn {}" );
    CHECK( scripts.LoadError( "scripts/a.luau", "require( '@lib/silent' )" )
               .find( "scripts/lib/silent.luau: a library returns what it shares" ) != string::npos );
    CHECK( scripts.LoadError( "scripts/b.luau", "require( '@lib/with_props' )" )
               .find( "props belong to entity scripts" ) != string::npos );
    CHECK( scripts.LoadError( "scripts/c.luau", "require( '@lib/with_callback' )" )
               .find( "on_update is a callback of entity scripts" ) != string::npos );
}

TEST_CASE( "A library is strict about globals like any file" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/leaky.luau", R"(
local leaky = {}
function leaky.touch() oops = 1 end
return leaky
)" );
    const auto unit = scripts.Load( "scripts/unit.luau", R"(
local leaky = require( "@lib/leaky" )
function on_hit( self ) leaky.touch() end
)" );
    const auto a = scripts.Make( unit );
    auto hit = scripts.Call( a, OnHit );
    REQUIRE_FALSE( hit );
    CHECK( hit.error().mMessage.find( "undeclared global 'oops'" ) != string::npos );
}

TEST_CASE( "A require cycle is an error that shows the cycle" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/a.luau", "return { b = require( './b' ) }" );
    scripts.AddLibrary( "scripts/lib/b.luau", "return { a = require( './a' ) }" );
    CHECK( scripts.LoadError( "scripts/main.luau", "require( '@lib/a' )" )
               .find( "require cycle: scripts/lib/a.luau -> scripts/lib/b.luau -> scripts/lib/a.luau" ) !=
           string::npos );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "An error inside a library fails the file that required it, with both places" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/broken.luau", "\n\nerror( 'bad tuning' )\nreturn {}" );
    const string error = scripts.LoadError( "scripts/main.luau", "local broken = require( '@lib/broken' )" );
    CHECK( error.find( "scripts/main.luau:1:" ) != string::npos );
    CHECK( error.find( "scripts/lib/broken.luau:3: bad tuning" ) != string::npos );

    // Not cached as loaded: fixed, it runs the next time.
    REQUIRE( scripts.Change( "scripts/lib/broken.luau", "return { fine = true }" ) );
    scripts.Load( "scripts/again.luau", "local broken = require( '@lib/broken' )" );
}

TEST_CASE( "require works inside callbacks and coroutines too" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/inventory.luau", cInventory );
    const auto unit = scripts.Load( "scripts/unit.luau", R"(
function on_start( self )
    start( function()
        wait( 1 )
        self.added = require( "@lib/inventory" ).added
    end )
end
function on_hit( self )
    require( "@lib/inventory" ).add( {}, "gem", 2 )
end
)" );
    const auto a = scripts.Make( unit );
    REQUIRE( scripts.Call( a, OnStart ) );
    REQUIRE( scripts.Call( a, OnHit ) );
    scripts.mRuntime.Tick( 1 );
    CHECK( scripts.Number( a, "added" ) == 2 );
    CHECK( lua_gettop( scripts.L() ) == 0 );
}

TEST_CASE( "A changed library reaches every script that required it, through other libraries too" )
{
    Project scripts;
    scripts.AddLibrary( "scripts/lib/tuning.luau", "return { speed = 1 }" );
    scripts.AddLibrary( "scripts/lib/movement.luau", R"(
local tuning = require( "./tuning" )
return { speed = function() return tuning.speed end }
)" );
    scripts.AddLibrary( "scripts/lib/unrelated.luau", R"(
record( "unrelated ran" )
return {}
)" );
    const auto direct = scripts.Load( "scripts/direct.luau", R"(
local tuning = require( "@lib/tuning" )
function on_update( self, dt ) self.speed = tuning.speed end
)" );
    const auto through = scripts.Load( "scripts/through.luau", R"(
local movement = require( "@lib/movement" )
function on_update( self, dt ) self.speed = movement.speed() end
)" );
    const auto other = scripts.Load( "scripts/other.luau", R"(
record( "other ran" )
local unrelated = require( "@lib/unrelated" )
function on_update( self, dt ) self.calls = ( self.calls or 0 ) + 1 end
)" );
    const auto a = scripts.Make( direct );
    const auto b = scripts.Make( through );
    const auto c = scripts.Make( other );
    scripts.Set( a, "kept", true );
    for ( const auto handle : { a, b, c } )
        REQUIRE( scripts.Call( handle, OnUpdate ) );

    scripts.mRecorded.clear();
    REQUIRE( scripts.Change( "scripts/lib/tuning.luau", "return { speed = 5 }" ) );
    for ( const auto handle : { a, b, c } )
        REQUIRE( scripts.Call( handle, OnUpdate ) );
    CHECK( scripts.Number( a, "speed" ) == 5 );
    CHECK( scripts.Number( b, "speed" ) == 5 );
    // self survives the reload.
    scripts.mRuntime.PushSelf( a );
    lua_getfield( scripts.L(), -1, "kept" );
    CHECK( lua_toboolean( scripts.L(), -1 ) );
    lua_pop( scripts.L(), 2 );
    // A script that never required it is not run again, nor is an
    // unrelated library.
    CHECK( scripts.mRecorded.empty() );
    CHECK( scripts.Number( c, "calls" ) == 2 );
}

TEST_CASE( "require takes libraries the registry holds and never reads a file itself" )
{
    Project scripts;
    // On disk, but not loaded for this world.
    scripts.mFiles["scripts/lib/unloaded.luau"] = "return {}";
    CHECK( scripts.LoadError( "scripts/main.luau", "require( '@lib/unloaded' )" )
               .find( "no library scripts/lib/unloaded.luau is loaded" ) != string::npos );
    CHECK_FALSE( scripts.mRuntime.SetAlias( "Lib", "scripts/lib" ) );
    CHECK_FALSE( scripts.mRuntime.SetAlias( "up", "../outside" ) );
}
