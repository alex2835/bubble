#pragma once
#include "bubble/input/input_event.hpp"
#include "bubble/platform/window.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/pointer.hpp"
#include "bubble/types/utility.hpp"

struct SDL_Cursor;
struct SDL_Gamepad;

namespace bubble
{
// The mouse pointer's shapes, as the system draws them.
enum class Cursor : u8
{
    Arrow,
    // Over text that can be typed in.
    Text,
    Hand,
    Move,
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
    NotAllowed,
    // None at all, the mouse still moving it.
    Hidden,
};
constexpr u32 cCursors = static_cast<u32>( Cursor::Hidden ) + 1;

struct PlatformOptions
{
    // SDL's video driver by name, the system's own when empty. Tests take
    // "dummy": windows that are not on any screen, events pushed by hand.
    string mVideoDriver;
    // The mouse touches as well: touch controls tried where there is no
    // touchscreen.
    bool mMouseAsTouch = false;
};

// The system's side of the engine: SDL started, its events made the
// engine's own, windows, the frame loop. SDL is nowhere else. One a
// process, made by the app before anything that needs a window, and gone
// after them all.
class Platform
{
public:
    // A second one while the first is there is an error.
    static expected<Scope<Platform>, string> TryStart( const PlatformOptions& options = {} );
    ~Platform();
    Platform( const Platform& ) = delete;
    Platform& operator=( const Platform& ) = delete;

    expected<Window, string> TryOpenWindow( const WindowOptions& options = {} );

    // Where a game keeps what it saves: the user's own folder for it -
    // %APPDATA%\bubble\<game> on Windows, ~/.local/share on Linux - made if
    // it is not there. The error says why the system gives none.
    expected<OsPath, string> TrySaveFolder( string_view game ) const;

    // What happened since the call before, in order: input in the engine's
    // terms - for the UI first, then Input - while gamepads coming and
    // going are opened and closed here, and a request to quit is kept.
    span<const InputEvent> Poll();
    // The last window was closed, or the system asked the game to end.
    bool QuitRequested() const { return mQuitRequested; }
    // The same, taken: asked again only when asked again - an editor that
    // asks about what is not saved, and may stay.
    bool TakeQuitRequest() { return exchange( mQuitRequested, false ); }

    // The pointer over the windows from now on.
    void SetCursor( Cursor cursor );
    // A file or a folder opened as the system opens it: a script in the
    // editor people chose for it, a folder in the file manager.
    expected<void, string> TryOpen( const OsPath& path ) const;
    // The system's clipboard as text: empty when it holds none.
    string ClipboardText() const;
    void SetClipboardText( const string& text );

    // Calls `frame` with the seconds since the call before - 0 the first
    // time - until it returns false: a loop on the desktop, the browser's
    // animation frames on the web. There Run never returns, and what is on
    // the stack when it is called stays alive, as the page goes on.
    void Run( function<bool( f64 elapsed )> frame );

private:
    Platform() = default;

    vector<InputEvent> mEvents;
    // Opened as they come, by SDL's id: closed when they go, or with SDL.
    hmap<u32, SDL_Gamepad*> mGamepads;
    // The system's, made as they are first shown and freed with SDL.
    array<SDL_Cursor*, cCursors> mCursors{};
    Cursor mCursor = Cursor::Arrow;
    bool mQuitRequested = false;
};
}
