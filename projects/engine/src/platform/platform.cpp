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

span<const InputEvent> Platform::Poll()
{
    mEvents.clear();
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
