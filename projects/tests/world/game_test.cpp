// What a game's scripts ask of whatever runs it: the world stood still, the
// cursor held, the game quit.
#include "bubble/world/game.hpp"
#include "log_watch.hpp"
#include "scripts/script_world.hpp"
#include <doctest.h>

using namespace bubble;
using namespace bubble::test;

TEST_CASE( "A script pauses its world, holds the cursor and asks to quit; paused, no step runs" )
{
    ScriptWorld scripts;
    scripts.mApp.AddModule( GameModule() );
    const AssetId script = scripts.Write( "scripts/menu.luau", R"(
        function on_start( self )
            game.lock_cursor( true )
            record( "locked " .. tostring( game.cursor_locked() ) )
            game.pause( true )
            record( "paused " .. tostring( game.paused() ) )
        end
        function on_fixed_update( self, dt )
            record( "step" )
        end
        function on_update( self, dt )
            if game.paused() then
                game.quit()
            end
        end
    )" );
    Scene scene;
    ScriptWorld::Add( scene, "menu", script );
    World world( scripts.mApp, std::move( scene ) );
    scripts.Load( world );
    CHECK( scripts.mRecorded == vector<string>{ "locked true", "paused true" } );
    CHECK( world.Paused() );
    const Game& game = world.Get<Game>();
    CHECK( game.CursorLocked() );
    CHECK( game.QuitAsked() );

    // The frames go on, the steps do not.
    world.Frame( 0.5 );
    CHECK( scripts.mRecorded.size() == 2 );
    world.SetPaused( false );
    world.Frame( 0.05 );
    CHECK( scripts.mRecorded.back() == "step" );
}

TEST_CASE( "A script asks for another level: the loop takes it once; a level not set is an error" )
{
    ScriptWorld scripts;
    scripts.mApp.AddModule( GameModule() );
    const AssetId two = AssetId::Generate();
    scripts.mCatalog.Add( two, *AssetPath::TryFrom( "levels/two.scene" ) );
    const AssetId script = scripts.Write( "scripts/door.luau", R"(
        function on_start( self )
            game.load_level( asset( "levels/two.scene" ) )
            game.load_level( asset() )
        end
    )" );
    Scene scene;
    ScriptWorld::Add( scene, "door", script );
    World world( scripts.mApp, std::move( scene ) );
    LogWatch log;
    scripts.Load( world );
    CHECK( log.Saw( LogLevel::Error, { "door.luau:4:", "game.load_level: no level" } ) );
    // The last asked before the error stands; taken, it is gone.
    Game& game = world.Get<Game>();
    CHECK( game.TakeLevelAsked() == two );
    CHECK_FALSE( game.TakeLevelAsked() );
}
