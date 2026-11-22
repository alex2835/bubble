#include "bubble/platform/platform.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"
#include <SDL3/SDL.h>
// Only SDL_SetMainReady: main is the program's own.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace bubble
{
namespace
{
// SDL numbers mouse buttons from 1 and gamepad buttons and axes from 0 in
// the order of ours, so a number in range is ours as it is.
static_assert( SDL_BUTTON_LEFT == 1 and SDL_BUTTON_MIDDLE == 2 and SDL_BUTTON_RIGHT == 3 and SDL_BUTTON_X1 == 4 and
               SDL_BUTTON_X2 == 5 );
static_assert( SDL_GAMEPAD_BUTTON_SOUTH == static_cast<int>( GamepadButton::South ) and
               SDL_GAMEPAD_BUTTON_START == static_cast<int>( GamepadButton::Start ) and
               SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER == static_cast<int>( GamepadButton::RightShoulder ) and
               SDL_GAMEPAD_BUTTON_DPAD_RIGHT == static_cast<int>( GamepadButton::DpadRight ) );
static_assert( SDL_GAMEPAD_AXIS_LEFTX == static_cast<int>( GamepadAxis::LeftX ) and
               SDL_GAMEPAD_AXIS_RIGHTY == static_cast<int>( GamepadAxis::RightY ) and
               SDL_GAMEPAD_AXIS_RIGHT_TRIGGER == static_cast<int>( GamepadAxis::RightTrigger ) );

opt<MouseButton> TryMouseButton( u8 button )
{
    if ( button < 1 or button > cMouseButtons )
        return nullopt;
    return static_cast<MouseButton>( button - 1 );
}

opt<GamepadButton> TryGamepadButton( u8 button )
{
    if ( button >= cGamepadButtons )
        return nullopt;
    return static_cast<GamepadButton>( button );
}

opt<GamepadAxis> TryGamepadAxis( u8 axis )
{
    if ( axis >= cGamepadAxes )
        return nullopt;
    return static_cast<GamepadAxis>( axis );
}

// The seconds between one call and the one before: 0 the first time.
class FrameTimer
{
public:
    f64 Next()
    {
        const auto now = chrono::steady_clock::now();
        const f64 elapsed = mLast ? chrono::duration<f64>( now - *mLast ).count() : 0.0;
        mLast = now;
        return elapsed;
    }

private:
    opt<chrono::steady_clock::time_point> mLast;
};

#ifdef __EMSCRIPTEN__
struct BrowserLoop
{
    function<bool( f64 )> mFrame;
    FrameTimer mTimer;
};

void RunBrowserFrame( void* loop )
{
    BrowserLoop& browserLoop = *static_cast<BrowserLoop*>( loop );
    if ( not browserLoop.mFrame( browserLoop.mTimer.Next() ) )
        emscripten_cancel_main_loop();
}
#endif
}

expected<Scope<Platform>, string> Platform::TryStart( const PlatformOptions& options )
{
    if ( SDL_WasInit( SDL_INIT_VIDEO ) != 0 )
        return unexpected( "the platform is started already: there is one a process"s );
    // Our main is the program's own; SDL's would have said it is ready.
    SDL_SetMainReady();
    if ( not options.mVideoDriver.empty() )
        SDL_SetHint( SDL_HINT_VIDEO_DRIVER, options.mVideoDriver.c_str() );
    // A touch is a touch: Input and the UI take touches as they are, and a
    // finger is not also a mouse.
    SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "0" );
    SDL_SetHint( SDL_HINT_MOUSE_TOUCH_EVENTS, options.mMouseAsTouch ? "1" : "0" );
    if ( not SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMEPAD ) )
        return unexpected( format( "SDL did not start: {}", SDL_GetError() ) );
    return Scope<Platform>( new Platform() );
}

Platform::~Platform()
{
    for ( const auto& [id, gamepad] : mGamepads )
        SDL_CloseGamepad( gamepad );
    for ( SDL_Cursor* cursor : mCursors )
        if ( cursor != nullptr )
            SDL_DestroyCursor( cursor );
    SDL_Quit();
}

expected<OsPath, string> Platform::TrySaveFolder( string_view game ) const
{
    char* folder = SDL_GetPrefPath( "bubble", string( game ).c_str() );
    if ( folder == nullptr )
        return unexpected( format( "no folder to save {} in: {}", game, SDL_GetError() ) );
    const OsPath path = PathFromUtf8( folder );
    SDL_free( folder );
    return path;
}

expected<Window, string> Platform::TryOpenWindow( const WindowOptions& options )
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if ( options.mResizable )
        flags |= SDL_WINDOW_RESIZABLE;
    SDL_Window* window = SDL_CreateWindow( options.mTitle.c_str(), options.mSize.x, options.mSize.y, flags );
    if ( window == nullptr )
        return unexpected( format( "no window \"{}\": {}", options.mTitle, SDL_GetError() ) );
    return Window( window );
}

struct Platform::Dialogs
{
    mutex mMutex;
    u32 mNext = 0;
    // This thread's alone.
    hmap<u32, function<void( opt<OsPath> )>> mWaiting;
    // Under the mutex: the system answers on a thread of its own.
    vector<pair<u32, opt<OsPath>>> mAnswered;
};

struct Platform::Asked
{
    Ref<Dialogs> mDialogs;
    u32 mId = 0;
    // A file dialog's filter, which SDL reads until it answers.
    string mKind;
    string mPattern;
    SDL_DialogFileFilter mFilter{};
};

Platform::Asked* Platform::Ask( function<void( opt<OsPath> )> chosen )
{
    if ( not mDialogs )
        mDialogs = CreateRef<Dialogs>();
    const u32 id = mDialogs->mNext++;
    mDialogs->mWaiting[id] = std::move( chosen );
    // Kept by SDL until it answers, which frees it.
    auto* asked = new Asked();
    asked->mDialogs = mDialogs;
    asked->mId = id;
    return asked;
}

void Platform::Answered( void* asked, const char* const* paths )
{
    const Scope<Asked> answered( static_cast<Asked*>( asked ) );
    opt<OsPath> pathMaybe;
    if ( paths == nullptr )
        LogError( "the dialog: {}", SDL_GetError() );
    else if ( paths[0] != nullptr )
        pathMaybe = PathFromUtf8( paths[0] );
    const lock_guard lock( answered->mDialogs->mMutex );
    answered->mDialogs->mAnswered.emplace_back( answered->mId, std::move( pathMaybe ) );
}

void Platform::ChooseFolder( const Window& window, const OsPath& from, function<void( opt<OsPath> )> chosen )
{
#ifdef __EMSCRIPTEN__
    (void)window;
    (void)from;
    chosen( nullopt );
#else
    const string start = from.empty() ? string() : PathToUtf8( from );
    SDL_ShowOpenFolderDialog( []( void* asked, const char* const* paths, int ) { Answered( asked, paths ); },
                              Ask( std::move( chosen ) ), window.mWindow, start.empty() ? nullptr : start.c_str(),
                              false );
#endif
}

void Platform::ChooseFile( const Window& window,
                           const OsPath& from,
                           string_view kind,
                           string_view pattern,
                           function<void( opt<OsPath> )> chosen )
{
#ifdef __EMSCRIPTEN__
    (void)window;
    (void)from;
    (void)kind;
    (void)pattern;
    chosen( nullopt );
#else
    const string start = from.empty() ? string() : PathToUtf8( from );
    Asked* asked = Ask( std::move( chosen ) );
    asked->mKind = kind;
    asked->mPattern = pattern;
    asked->mFilter = SDL_DialogFileFilter{ .name = asked->mKind.c_str(), .pattern = asked->mPattern.c_str() };
    SDL_ShowOpenFileDialog( []( void* userdata, const char* const* paths, int ) { Answered( userdata, paths ); }, asked,
                            window.mWindow, &asked->mFilter, 1, start.empty() ? nullptr : start.c_str(), false );
#endif
}

span<const InputEvent> Platform::Poll()
{
    mEvents.clear();
    if ( mDialogs )
    {
        vector<pair<u32, opt<OsPath>>> answered;
        {
            const lock_guard lock( mDialogs->mMutex );
            answered = std::exchange( mDialogs->mAnswered, {} );
        }
        for ( auto& [id, folderMaybe] : answered )
            if ( auto waiting = mDialogs->mWaiting.extract( id ) )
                waiting.mapped()( std::move( folderMaybe ) );
    }
    SDL_Event event;
    while ( SDL_PollEvent( &event ) )
    {
        switch ( event.type )
        {
            case SDL_EVENT_QUIT: mQuitRequested = true; break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                // Keys of no use to a game - a media key, a language's own -
                // are left out.
                if ( const auto keyMaybe = TryKeyFromCode( static_cast<u32>( event.key.scancode ) ) )
                {
                    if ( event.type == SDL_EVENT_KEY_DOWN )
                        mEvents.push_back( KeyDown{ .mKey = *keyMaybe, .mRepeat = event.key.repeat } );
                    else
                        mEvents.push_back( KeyUp{ .mKey = *keyMaybe } );
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                if ( event.text.text != nullptr and event.text.text[0] != 0 )
                    mEvents.push_back( TextInput{ .mText = event.text.text } );
                break;
            case SDL_EVENT_MOUSE_MOTION:
                mEvents.push_back( MouseMove{ .mPosition = vec2( event.motion.x, event.motion.y ),
                                              .mDelta = vec2( event.motion.xrel, event.motion.yrel ) } );
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if ( const auto buttonMaybe = TryMouseButton( event.button.button ) )
                {
                    const vec2 position( event.button.x, event.button.y );
                    if ( event.type == SDL_EVENT_MOUSE_BUTTON_DOWN )
                        mEvents.push_back( MouseDown{ .mButton = *buttonMaybe, .mPosition = position } );
                    else
                        mEvents.push_back( MouseUp{ .mButton = *buttonMaybe, .mPosition = position } );
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
            {
                const f32 turn = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                mEvents.push_back( MouseWheel{ .mDelta = vec2( event.wheel.x, event.wheel.y ) * turn } );
                break;
            }
            case SDL_EVENT_FINGER_DOWN:
                mEvents.push_back( TouchDown{ .mFinger = event.tfinger.fingerID,
                                              .mPosition = vec2( event.tfinger.x, event.tfinger.y ) } );
                break;
            case SDL_EVENT_FINGER_MOTION:
                mEvents.push_back( TouchMove{ .mFinger = event.tfinger.fingerID,
                                              .mPosition = vec2( event.tfinger.x, event.tfinger.y ),
                                              .mDelta = vec2( event.tfinger.dx, event.tfinger.dy ) } );
                break;
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED:
                mEvents.push_back( TouchUp{ .mFinger = event.tfinger.fingerID,
                                            .mPosition = vec2( event.tfinger.x, event.tfinger.y ) } );
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if ( SDL_Gamepad* gamepad = SDL_OpenGamepad( event.gdevice.which ) )
                {
                    mGamepads[event.gdevice.which] = gamepad;
                    mEvents.push_back( GamepadAdded{ .mGamepad = event.gdevice.which } );
                }
                else
                    LogWarning( "A gamepad came but would not open: {}", SDL_GetError() );
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if ( const auto found = mGamepads.find( event.gdevice.which ); found != mGamepads.end() )
                {
                    SDL_CloseGamepad( found->second );
                    mGamepads.erase( found );
                }
                mEvents.push_back( GamepadRemoved{ .mGamepad = event.gdevice.which } );
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                // The extra buttons some pads have - a share button, paddles -
                // are not ours yet.
                if ( const auto buttonMaybe = TryGamepadButton( event.gbutton.button ) )
                {
                    if ( event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN )
                        mEvents.push_back(
                            GamepadButtonDown{ .mGamepad = event.gbutton.which, .mButton = *buttonMaybe } );
                    else
                        mEvents.push_back(
                            GamepadButtonUp{ .mGamepad = event.gbutton.which, .mButton = *buttonMaybe } );
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if ( const auto axisMaybe = TryGamepadAxis( event.gaxis.axis ) )
                    mEvents.push_back(
                        GamepadAxisMove{ .mGamepad = event.gaxis.which,
                                         .mAxis = *axisMaybe,
                                         .mValue = clamp( event.gaxis.value / 32767.0f, -1.0f, 1.0f ) } );
                break;
            default: break;
        }
    }
    return mEvents;
}

void Platform::SetCursor( Cursor cursor )
{
    if ( cursor == mCursor )
        return;
    mCursor = cursor;
    if ( cursor == Cursor::Hidden )
    {
        SDL_HideCursor();
        return;
    }
    static constexpr array<SDL_SystemCursor, cCursors - 1> cShapes{
        SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_TEXT,        SDL_SYSTEM_CURSOR_POINTER,
        SDL_SYSTEM_CURSOR_MOVE,        SDL_SYSTEM_CURSOR_NS_RESIZE,   SDL_SYSTEM_CURSOR_EW_RESIZE,
        SDL_SYSTEM_CURSOR_NESW_RESIZE, SDL_SYSTEM_CURSOR_NWSE_RESIZE, SDL_SYSTEM_CURSOR_NOT_ALLOWED,
    };
    const u32 at = static_cast<u32>( cursor );
    if ( mCursors[at] == nullptr )
        mCursors[at] = SDL_CreateSystemCursor( cShapes[at] );
    // A system with no such shape keeps the one it shows.
    if ( mCursors[at] != nullptr )
        SDL_SetCursor( mCursors[at] );
    SDL_ShowCursor();
}

expected<void, string> Platform::TryOpen( const OsPath& path ) const
{
    string url = PathToUtf8( path );
    ranges::replace( url, '\\', '/' );
    url = ( url.starts_with( '/' ) ? "file://" : "file:///" ) + url;
    if ( not SDL_OpenURL( url.c_str() ) )
        return unexpected( format( "{} would not open: {}", PathToUtf8( path ), SDL_GetError() ) );
    return {};
}

string Platform::ClipboardText() const
{
    char* text = SDL_GetClipboardText();
    if ( text == nullptr )
        return {};
    string copy( text );
    SDL_free( text );
    return copy;
}

void Platform::SetClipboardText( const string& text )
{
    if ( not SDL_SetClipboardText( text.c_str() ) )
        LogWarning( "The clipboard would not take the text: {}", SDL_GetError() );
}

void Platform::Run( function<bool( f64 elapsed )> frame )
{
#ifdef __EMSCRIPTEN__
    // The browser calls a frame when it is about to draw; this function
    // ends by an exception emscripten throws past it, which leaves the
    // stack as it is - the loop's state included.
    BrowserLoop loop{ .mFrame = std::move( frame ), .mTimer = {} };
    emscripten_set_main_loop_arg( &RunBrowserFrame, &loop, 0, true );
#else
    FrameTimer timer;
    while ( frame( timer.Next() ) )
    {
    }
#endif
}
}
