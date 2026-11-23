#include "bubble/world/game.hpp"
#include "bubble/scripts/script_api.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
namespace
{
class GameState final : public Module
{
public:
    explicit GameState( World& world ) { world.GetServices().Add( mGame ); }

private:
    Game mGame;
};

void ScriptsOfGame( ScriptApi& api )
{
    api.AddGlobals(
        []( LuaState& lua, World& world ) {
            LuaTable game = lua.NewTable();
            game["pause"] = [&world]( bool paused ) { world.SetPaused( paused ); };
            game["paused"] = [&world]() { return world.Paused(); };
            game["lock_cursor"] = [&world]( bool locked ) { world.Get<Game>().LockCursor( locked ); };
            game["cursor_locked"] = [&world]() { return world.Get<Game>().CursorLocked(); };
            game["load_level"] = [&world]( const AssetId& levelId ) {
                if ( not levelId )
                    throw LuaError( "game.load_level: no level - an asset() prop the scene has not set" );
                world.Get<Game>().LoadLevel( levelId );
            };
            game["quit"] = [&world]() { world.Get<Game>().AskToQuit(); };
            lua.Globals()["game"] = game;
        },
        "declare game: {\n"
        "    pause: (paused: boolean) -> (),\n"
        "    paused: () -> boolean,\n"
        "    lock_cursor: (locked: boolean) -> (),\n"
        "    cursor_locked: () -> boolean,\n"
        "    load_level: (level: asset) -> (),\n"
        "    quit: () -> (),\n"
        "}\n" );
}
}

ModuleType GameModule()
{
    return ModuleType{
        .mName = "game",
        .mDescribe = []( Reflection&, ComponentTypes& ) {},
        .mCreate = []( World& world ) -> Scope<Module> { return CreateScope<GameState>( world ); },
        .mScripts = &ScriptsOfGame,
    };
}
}
