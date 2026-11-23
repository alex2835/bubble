#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/types/utility.hpp"
#include "bubble/world/module.hpp"

namespace bubble
{
// What a game's scripts ask of whatever runs it: to stand the world still -
// a pause menu -, to hold the cursor for a camera the mouse turns, to go to
// another level, to quit. A world's service: the world pauses at once; the
// cursor, the level and quitting are the loop's to read each frame, and a
// world nobody runs ignores them.
class Game
{
public:
    bool CursorLocked() const { return mCursorLocked; }
    bool QuitAsked() const { return mQuitAsked; }
    void LockCursor( bool locked ) { mCursorLocked = locked; }
    void AskToQuit() { mQuitAsked = true; }
    // The level to go on in: the loop makes a world of it after this frame -
    // a world of its own, scripts started anew; what lasts across levels is
    // the scripts' progress. Asked twice in a frame, the last one stands.
    void LoadLevel( const AssetId& levelId ) { mLevelAskedMaybe = levelId; }
    // The level asked for, once: the loop takes it.
    opt<AssetId> TakeLevelAsked() { return exchange( mLevelAskedMaybe, nullopt ); }

private:
    opt<AssetId> mLevelAskedMaybe;
    bool mCursorLocked = false;
    bool mQuitAsked = false;
};

// Game, and scripts' game.pause( on ), game.paused(), game.lock_cursor( on ),
// game.cursor_locked(), game.load_level( level ), game.quit().
ModuleType GameModule();
}
