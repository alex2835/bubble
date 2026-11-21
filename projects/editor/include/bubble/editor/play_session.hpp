#pragma once
#include "bubble/editing/document.hpp"
#include "bubble/input/input.hpp"
#include "bubble/input/input_event.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
class EditorProject;
class DocumentWorld;

// A game played from a scene's document, as bubble_play plays a level: the
// scene copied into a document of its own - open in the workspace, active,
// shown, the tree's and the inspector's, edits to it lost with it - and a
// world of every module over the copy, stepped each frame, the project's
// actions coming from the player's input. The document it was played from
// is not touched; the session gone, the copy is closed and that document is
// active again.
class PlaySession
{
public:
    PlaySession( EditorProject& project, Document& source );
    ~PlaySession();
    PlaySession( const PlaySession& ) = delete;
    PlaySession& operator=( const PlaySession& ) = delete;

    Document& Played() { return mPlayed; }
    DocumentWorld& View();
    World& GetWorld();
    // What its tab says: the file it was played from.
    const string& Title() const { return mTitle; }

    // Each frame: the world's frame, what `elapsed` makes due - nothing
    // while it is paused.
    void Frame( f64 elapsed );
    // Where the game is shown, in the window's units: its screen, and what
    // the mouse's positions are made relative to.
    void SetScreen( vec2 corner, vec2 size );
    // The player's input, as the platform polled it: the game's UI first,
    // then its actions.
    void Handle( const InputEvent& event );
    // The player turned away: what they held goes up, as if let go.
    void Release();

private:
    EditorProject& mProject;
    string mTitle;
    Document& mPlayed;
    Input mInput;
    vec2 mCorner{ 0.0f };
    vec2 mSize{ 1280.0f, 720.0f };
    // What went down through Handle and has not come up: Release lets it go.
    vector<Key> mKeysDown;
    vector<MouseButton> mButtonsDown;
};
}
