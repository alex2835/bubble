// The platform on SDL's dummy video driver: a window on no screen, SDL's
// events pushed by hand and read back as the engine's. Not on the web,
// where the tests run under node with no page to open a window in.
#ifndef __EMSCRIPTEN__
#include "bubble/input/input.hpp"
#include "bubble/platform/platform.hpp"
#include <SDL3/SDL.h>
#include <doctest.h>

using namespace bubble;

namespace
{
// Not one SDL gives a real pad: the test machine may have some plugged in.
constexpr u32 cPad = 4242;

Scope<Platform> StartDummy()
{
    auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = "dummy", .mMouseAsTouch = false } );
    REQUIRE_MESSAGE( platformMaybe, platformMaybe.error() );
    return std::move( *platformMaybe );
}

void Push( SDL_Event event )
{
    REQUIRE_MESSAGE( SDL_PushEvent( &event ), SDL_GetError() );
}

void PushKey( SDL_EventType type, SDL_Scancode code, bool repeat = false )
{
    SDL_Event event{};
    event.type = type;
    event.key.scancode = code;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.repeat = repeat;
    Push( event );
}

void PushFinger( SDL_EventType type, SDL_FingerID finger, f32 x, f32 y, f32 dx = 0.0f, f32 dy = 0.0f )
{
    SDL_Event event{};
    event.type = type;
    event.tfinger.fingerID = finger;
    event.tfinger.x = x;
    event.tfinger.y = y;
    event.tfinger.dx = dx;
    event.tfinger.dy = dy;
    Push( event );
}

// What Poll gave, with the pads that are really there left out.
vector<InputEvent> Polled( Platform& platform )
{
    vector<InputEvent> events;
    for ( const InputEvent& event : platform.Poll() )
    {
        const bool otherPad = visit(
            []( const auto& e ) {
                if constexpr ( requires { e.mGamepad; } )
                    return e.mGamepad != cPad;
                else
                    return false;
            },
            event );
        if ( not otherPad )
            events.push_back( event );
    }
    return events;
}
}

TEST_CASE( "A platform opens windows, and there is one platform a process" )
{
    const Scope<Platform> platform = StartDummy();
    CHECK( Platform::TryStart().error() == "the platform is started already: there is one a process" );

    auto windowMaybe =
        platform->TryOpenWindow( WindowOptions{ .mTitle = "test", .mSize = { 320, 200 }, .mResizable = false } );
    REQUIRE_MESSAGE( windowMaybe, windowMaybe.error() );
    CHECK( windowMaybe->Size() == ivec2( 320, 200 ) );
    CHECK( windowMaybe->PixelSize().x >= 320 );
    CHECK( windowMaybe->Scale() > 0.0f );
    Window moved = std::move( *windowMaybe );
    moved.SetTitle( "moved" );
    CHECK( moved.Size() == ivec2( 320, 200 ) );
}

TEST_CASE( "SDL's keys, mouse and touches come out as the engine's events" )
{
    const Scope<Platform> platform = StartDummy();
    PushKey( SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE );
    PushKey( SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, true );
    // A key ours leave out.
    PushKey( SDL_EVENT_KEY_DOWN, SDL_SCANCODE_NONUSHASH );
    PushKey( SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE );

    SDL_Event button{};
    button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button.button.button = SDL_BUTTON_RIGHT;
    button.button.x = 10.0f;
    button.button.y = 20.0f;
    Push( button );
    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 1.0f;
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    Push( wheel );

    PushFinger( SDL_EVENT_FINGER_DOWN, 7, 0.25f, 0.5f );
    PushFinger( SDL_EVENT_FINGER_MOTION, 7, 0.5f, 0.5f, 0.25f, 0.0f );
    // Taken away by the system: it lifts all the same.
    PushFinger( SDL_EVENT_FINGER_CANCELED, 7, 0.5f, 0.5f );

    const vector<InputEvent> wanted{
        KeyDown{ .mKey = Key::Space, .mRepeat = false },
        KeyDown{ .mKey = Key::Space, .mRepeat = true },
        KeyUp{ .mKey = Key::Space },
        MouseDown{ .mButton = MouseButton::Right, .mPosition = { 10.0f, 20.0f } },
        MouseWheel{ .mDelta = { 0.0f, -1.0f } },
        TouchDown{ .mFinger = 7, .mPosition = { 0.25f, 0.5f } },
        TouchMove{ .mFinger = 7, .mPosition = { 0.5f, 0.5f }, .mDelta = { 0.25f, 0.0f } },
        TouchUp{ .mFinger = 7, .mPosition = { 0.5f, 0.5f } },
    };
    CHECK( Polled( *platform ) == wanted );
    CHECK( Polled( *platform ).empty() );
    CHECK_FALSE( platform->QuitRequested() );
}

TEST_CASE( "Text typed comes as UTF-8 while the window takes text, and the clipboard holds text" )
{
    const Scope<Platform> platform = StartDummy();
    auto windowMaybe = platform->TryOpenWindow( WindowOptions{ .mTitle = "text", .mSize = { 64, 64 } } );
    REQUIRE_MESSAGE( windowMaybe, windowMaybe.error() );
    windowMaybe->SetTextInput( true );
    SDL_Event text{};
    text.type = SDL_EVENT_TEXT_INPUT;
    text.text.text = "ёж";
    Push( text );
    // An empty one says nothing.
    text.text.text = "";
    Push( text );
    CHECK( Polled( *platform ) == vector<InputEvent>{ TextInput{ .mText = "ёж" } } );
    windowMaybe->SetTextInput( false );

    platform->SetClipboardText( "привет" );
    CHECK( platform->ClipboardText() == "привет" );
    // A shape set twice, and hidden, is no error.
    platform->SetCursor( Cursor::Text );
    platform->SetCursor( Cursor::Text );
    platform->SetCursor( Cursor::Hidden );
    platform->SetCursor( Cursor::Arrow );
}

TEST_CASE( "Gamepad buttons and axes come out scaled, the extra buttons left out" )
{
    const Scope<Platform> platform = StartDummy();
    const auto pushButton = []( SDL_GamepadButton which ) {
        SDL_Event event{};
        event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        event.gbutton.which = cPad;
        event.gbutton.button = static_cast<u8>( which );
        event.gbutton.down = true;
        Push( event );
    };
    const auto pushAxis = []( SDL_GamepadAxis axis, i16 value ) {
        SDL_Event event{};
        event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        event.gaxis.which = cPad;
        event.gaxis.axis = static_cast<u8>( axis );
        event.gaxis.value = value;
        Push( event );
    };
    pushButton( SDL_GAMEPAD_BUTTON_SOUTH );
    pushButton( SDL_GAMEPAD_BUTTON_MISC1 );
    pushAxis( SDL_GAMEPAD_AXIS_LEFTX, 32767 );
    pushAxis( SDL_GAMEPAD_AXIS_LEFTY, -32768 );
    pushAxis( SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0 );

    const vector<InputEvent> wanted{
        GamepadButtonDown{ .mGamepad = cPad, .mButton = GamepadButton::South },
        GamepadAxisMove{ .mGamepad = cPad, .mAxis = GamepadAxis::LeftX, .mValue = 1.0f },
        GamepadAxisMove{ .mGamepad = cPad, .mAxis = GamepadAxis::LeftY, .mValue = -1.0f },
        GamepadAxisMove{ .mGamepad = cPad, .mAxis = GamepadAxis::RightTrigger, .mValue = 0.0f },
    };
    CHECK( Polled( *platform ) == wanted );
}

TEST_CASE( "A request to quit is kept, and Run calls frames until one says stop" )
{
    const Scope<Platform> platform = StartDummy();
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    Push( quit );
    CHECK( Polled( *platform ).empty() );
    CHECK( platform->QuitRequested() );

    vector<f64> elapsed;
    platform->Run( [&]( f64 seconds ) {
        elapsed.push_back( seconds );
        return elapsed.size() < 3;
    } );
    REQUIRE( elapsed.size() == 3 );
    CHECK( elapsed[0] == 0.0 );
    CHECK( elapsed[1] >= 0.0 );
    CHECK( elapsed[2] >= 0.0 );
}

TEST_CASE( "An action fires from the keyboard and from a touch, through the platform" )
{
    const Scope<Platform> platform = StartDummy();
    ActionMap map;
    map.Bind( "jump", Key::Space ).Bind( "jump", TouchArea{ .mMin = { 0.5f, 0.0f }, .mMax = { 1.0f, 1.0f } } );
    Input input( std::move( map ) );
    const auto frame = [&] {
        for ( const InputEvent& event : platform->Poll() )
            input.Handle( event );
        return input.LatchStep();
    };

    PushKey( SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE );
    CHECK( frame().Pressed( "jump"_n ) );
    PushKey( SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE );
    CHECK( frame().Released( "jump"_n ) );

    PushFinger( SDL_EVENT_FINGER_DOWN, 3, 0.8f, 0.4f );
    CHECK( frame().Pressed( "jump"_n ) );
    PushFinger( SDL_EVENT_FINGER_UP, 3, 0.8f, 0.4f );
    CHECK( frame().Released( "jump"_n ) );
}
#endif
