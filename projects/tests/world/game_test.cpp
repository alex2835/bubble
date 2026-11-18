// What a game's scripts ask of whatever runs it: the world stood still, the
// cursor held, the game quit.
#include "bubble/world/game.hpp"
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
