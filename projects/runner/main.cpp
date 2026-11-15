// The runner: a game on its own. App, the project and its levels come with
// their stages; for now a window and the actions that fire in it, logged -
// stage 5's check by hand. The keyboard, a gamepad and touches drive "jump"
// and "move_x"; the mouse touches too, for a screen without touch.
#include "bubble/core/log.hpp"
#include "bubble/input/input.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/types/thread.hpp"

using namespace bubble;

namespace
{
ActionMap Controls()
{
    // The right half of the window jumps; its left quarters move.
    const TouchArea rightHalf{ .mMin = { 0.5f, 0.0f }, .mMax = { 1.0f, 1.0f } };
    const TouchArea leftQuarter{ .mMin = { 0.0f, 0.0f }, .mMax = { 0.25f, 1.0f } };
    const TouchArea secondQuarter{ .mMin = { 0.25f, 0.0f }, .mMax = { 0.5f, 1.0f } };
    ActionMap map;
    map.Bind( "jump", Key::Space ).Bind( "jump", GamepadButton::South ).Bind( "jump", rightHalf );
    map.Bind( "move_x", Key::A, -1.0f )
        .Bind( "move_x", Key::D )
        .Bind( "move_x", Key::Left, -1.0f )
        .Bind( "move_x", Key::Right )
        .Bind( "move_x", GamepadAxis::LeftX )
        .Bind( "move_x", leftQuarter, -1.0f )
        .Bind( "move_x", secondQuarter );
    return map;
}
}

int main()
{
    const auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = {}, .mMouseAsTouch = true } );
    if ( not platformMaybe )
    {
        LogError( "{}", platformMaybe.error() );
        return 1;
    }
    Platform& platform = **platformMaybe;
    const auto windowMaybe =
        platform.TryOpenWindow( WindowOptions{ .mTitle = "bubble runner", .mSize = { 960, 540 }, .mResizable = true } );
    if ( not windowMaybe )
    {
        LogError( "{}", windowMaybe.error() );
        return 1;
    }

    Input input( Controls() );
    f32 lastMove = 0.0f;
    platform.Run( [&]( f64 ) {
        for ( const InputEvent& event : platform.Poll() )
            input.Handle( event );
        const Actions actions = input.LatchFrame();
        if ( actions.Pressed( "jump"_n ) )
            LogInfo( "jump" );
        if ( const f32 move = actions.Axis( "move_x"_n ); move != lastMove )
        {
            LogInfo( "move_x {:.2f}", move );
            lastMove = move;
        }
#ifndef __EMSCRIPTEN__
        // Until the renderer's present waits for the display.
        this_thread::sleep_for( chrono::milliseconds( 10 ) );
#endif
        return not platform.QuitRequested();
    } );
    return 0;
}
