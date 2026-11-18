#pragma once
#include "bubble/world/module.hpp"

namespace bubble
{
// What a game's scripts ask of whatever runs it: to stand the world still -
// a pause menu -, to hold the cursor for a camera the mouse turns, to quit.
// A world's service: the world pauses at once; the cursor and quitting are
// the loop's to read each frame, and a world nobody runs ignores them.
class Game
{
public:
    bool CursorLocked() const { return mCursorLocked; }
    bool QuitAsked() const { return mQuitAsked; }
    void LockCursor( bool locked ) { mCursorLocked = locked; }
    void AskToQuit() { mQuitAsked = true; }

private:
    bool mCursorLocked = false;
    bool mQuitAsked = false;
};

// Game, and scripts' game.pause( on ), game.paused(), game.lock_cursor( on ),
// game.cursor_locked(), game.quit().
ModuleType GameModule();
}
