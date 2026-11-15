// The runner: a game on its own. App, the project and its levels come with
// their stages; for now a window the GPU clears to a color the actions
// change, and the actions logged - stages 5 and 6 checked by hand. The
// keyboard, a gamepad and touches drive "jump" and "move_x"; the mouse
// touches too, for a screen without touch.
#include "bubble/core/log.hpp"
#include "bubble/input/input.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/types/algorithm.hpp"
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

// Night blue; red to the right, blue to the left, lighter while jumping.
Color Background( const Actions& actions )
{
    const f32 move = actions.Axis( "move_x"_n );
    const f32 jump = actions.Down( "jump"_n ) ? 0.4f : 0.0f;
    return Color{ .mR = 0.1f + 0.5f * max( move, 0.0f ) + jump,
                  .mG = 0.12f + jump,
                  .mB = 0.25f + 0.5f * max( -move, 0.0f ) + jump,
                  .mA = 1.0f };
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
    const Window& window = *windowMaybe;

    // The GPU after the window, and gone before it.
    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
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

        if ( not gpu )
        {
            auto finishedMaybe = startup->TryFinish();
            if ( not finishedMaybe )
                return true;
            if ( not *finishedMaybe )
            {
                LogError( "{}", finishedMaybe->error() );
                return false;
            }
            gpu = std::move( **finishedMaybe );
        }
        gpu->Poll();
        GpuSurface& surface = *gpu->TrySurface();
        surface.Resize( window.PixelSize() );
        // Presenting waits for the display, which paces the loop.
        if ( auto viewMaybe = surface.TryAcquire() )
        {
            gpu->Clear( viewMaybe->Get(), Background( actions ) );
            surface.Present();
        }
#ifndef __EMSCRIPTEN__
        // Minimized: nothing to present, nothing to wait for.
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
#endif
        return not platform.QuitRequested();
    } );
    return 0;
}
