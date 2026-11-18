// Actions made of the devices' state and latched for fixed steps and
// frames: the engine's side of "an action fires from the keyboard and from
// a touch".
#include "bubble/input/input.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr TouchArea cRightHalf{ .mMin = { 0.5f, 0.0f }, .mMax = { 1.0f, 1.0f } };

ActionMap Controls()
{
    ActionMap map;
    map.Bind( "jump", Key::Space ).Bind( "jump", GamepadButton::South ).Bind( "jump", cRightHalf );
    map.Bind( "move_x", Key::A, -1.0f ).Bind( "move_x", Key::D ).Bind( "move_x", GamepadAxis::LeftX );
    map.Bind( "fire", MouseButton::Left );
    return map;
}
}

TEST_CASE( "A key drives its action: pressed once, down while held, released once" )
{
    Input input( Controls() );
    CHECK_FALSE( input.LatchStep().Down( "jump"_n ) );

    input.Handle( KeyDown{ .mKey = Key::Space, .mRepeat = false } );
    const Actions first = input.LatchStep();
    CHECK( first.Pressed( "jump"_n ) );
    CHECK( first.Down( "jump"_n ) );
    CHECK( first.Axis( "jump"_n ) == 1.0f );
    CHECK_FALSE( first.Released( "jump"_n ) );

    // The system typing it again changes nothing.
    input.Handle( KeyDown{ .mKey = Key::Space, .mRepeat = true } );
    const Actions held = input.LatchStep();
    CHECK_FALSE( held.Pressed( "jump"_n ) );
    CHECK( held.Down( "jump"_n ) );

    input.Handle( KeyUp{ .mKey = Key::Space } );
    const Actions last = input.LatchStep();
    CHECK( last.Released( "jump"_n ) );
    CHECK_FALSE( last.Down( "jump"_n ) );
    CHECK( input.LatchStep().States()[0] == ActionState{} );

    input.Handle( MouseDown{ .mButton = MouseButton::Left, .mPosition = { 4.0f, 2.0f } } );
    CHECK( input.LatchStep().Pressed( "fire"_n ) );
}

TEST_CASE( "A press is seen by exactly one fixed step and one frame, at any frame rate" )
{
    Input input( Controls() );

    // A tap between two steps: down and up again before either.
    input.Handle( KeyDown{ .mKey = Key::Space, .mRepeat = false } );
    input.Handle( KeyUp{ .mKey = Key::Space } );
    const Actions tapped = input.LatchStep();
    CHECK( tapped.Pressed( "jump"_n ) );
    CHECK( tapped.Released( "jump"_n ) );
    CHECK_FALSE( tapped.Down( "jump"_n ) );
    CHECK_FALSE( input.LatchStep().Pressed( "jump"_n ) );

    // A slow frame runs three steps: the first sees the press.
    input.Handle( KeyDown{ .mKey = Key::Space, .mRepeat = false } );
    CHECK( input.LatchStep().Pressed( "jump"_n ) );
    CHECK_FALSE( input.LatchStep().Pressed( "jump"_n ) );
    CHECK_FALSE( input.LatchStep().Pressed( "jump"_n ) );
    // The frame has its own latch: it sees the press too, once.
    CHECK( input.LatchFrame().Pressed( "jump"_n ) );
    CHECK_FALSE( input.LatchFrame().Pressed( "jump"_n ) );

    // Fast frames with no step between: the press waits for the next step.
    input.Handle( KeyUp{ .mKey = Key::Space } );
    CHECK( input.LatchFrame().Released( "jump"_n ) );
    CHECK_FALSE( input.LatchFrame().Released( "jump"_n ) );
    CHECK( input.LatchStep().Released( "jump"_n ) );
}

TEST_CASE( "Opposite keys cancel, and a stick drives an axis past its dead zone" )
{
    Input input( Controls() );
    input.Handle( KeyDown{ .mKey = Key::A, .mRepeat = false } );
    CHECK( input.LatchStep().Axis( "move_x"_n ) == -1.0f );
    input.Handle( KeyDown{ .mKey = Key::D, .mRepeat = false } );
    const Actions both = input.LatchStep();
    CHECK( both.Axis( "move_x"_n ) == 0.0f );
    CHECK( both.Released( "move_x"_n ) );
    input.Handle( KeyUp{ .mKey = Key::A } );
    input.Handle( KeyUp{ .mKey = Key::D } );

    input.Handle( GamepadAdded{ .mGamepad = 1 } );
    input.Handle( GamepadAxisMove{ .mGamepad = 1, .mAxis = GamepadAxis::LeftX, .mValue = 0.15f } );
    CHECK( input.LatchStep().Axis( "move_x"_n ) == 0.0f );
    input.Handle( GamepadAxisMove{ .mGamepad = 1, .mAxis = GamepadAxis::LeftX, .mValue = -0.4f } );
    CHECK( input.LatchStep().Axis( "move_x"_n ) == doctest::Approx( -0.25f ) );
    input.Handle( GamepadAxisMove{ .mGamepad = 1, .mAxis = GamepadAxis::LeftX, .mValue = 1.0f } );
    const Actions pushed = input.LatchStep();
    CHECK( pushed.Axis( "move_x"_n ) == 1.0f );
    CHECK( pushed.Pressed( "move_x"_n ) );

    // Of two pads, the one pushed furthest; a pad unplugged lets go of all
    // it held.
    input.Handle( GamepadAxisMove{ .mGamepad = 2, .mAxis = GamepadAxis::LeftX, .mValue = -0.4f } );
    CHECK( input.LatchStep().Axis( "move_x"_n ) == 1.0f );
    input.Handle( GamepadButtonDown{ .mGamepad = 1, .mButton = GamepadButton::South } );
    CHECK( input.LatchStep().Pressed( "jump"_n ) );
    input.Handle( GamepadRemoved{ .mGamepad = 1 } );
    const Actions unplugged = input.LatchStep();
    CHECK( unplugged.Released( "jump"_n ) );
    CHECK( unplugged.Axis( "move_x"_n ) == doctest::Approx( -0.25f ) );
}

TEST_CASE( "The mouse drives an axis by how far it went since the last latch, the steps' and the frames' apart" )
{
    ActionMap map;
    map.Bind( "look_x", MouseAxis::X, 0.5f ).Bind( "look_x", Key::Right ).Bind( "zoom", MouseAxis::Wheel, -1.0f );
    Input input( map );
    input.Handle( MouseMove{ .mPosition = { 10.0f, 0.0f }, .mDelta = { 10.0f, 3.0f } } );
    input.Handle( MouseMove{ .mPosition = { 14.0f, 0.0f }, .mDelta = { 4.0f, -1.0f } } );
    input.Handle( MouseWheel{ .mDelta = { 0.0f, 2.0f } } );
    // Never clamped: a camera turns by it.
    const Actions step = input.LatchStep();
    CHECK( step.Axis( "look_x"_n ) == 7.0f );
    CHECK( step.Axis( "zoom"_n ) == -2.0f );
    // Taken by the step, and still there for the frame.
    CHECK( input.LatchStep().Axis( "look_x"_n ) == 0.0f );
    CHECK( input.LatchFrame().Axis( "look_x"_n ) == 7.0f );
    CHECK( input.LatchFrame().Axis( "look_x"_n ) == 0.0f );

    // On top of what the action's other bindings hold.
    input.Handle( KeyDown{ .mKey = Key::Right, .mRepeat = false } );
    input.Handle( MouseMove{ .mPosition = { 12.0f, 0.0f }, .mDelta = { -2.0f, 0.0f } } );
    CHECK( input.LatchStep().Axis( "look_x"_n ) == 0.0f );
    CHECK( input.LatchStep().Axis( "look_x"_n ) == 1.0f );
}

TEST_CASE( "A touch holds the action of the area it started in until the finger lifts" )
{
    Input input( Controls() );
    input.Handle( TouchDown{ .mFinger = 7, .mPosition = { 0.75f, 0.5f } } );
    CHECK( input.LatchStep().Pressed( "jump"_n ) );
    // Wherever it goes meanwhile.
    input.Handle( TouchMove{ .mFinger = 7, .mPosition = { 0.1f, 0.5f }, .mDelta = { -0.65f, 0.0f } } );
    CHECK( input.LatchStep().Down( "jump"_n ) );

    // A second finger on the same button; the first lifts, the second holds.
    input.Handle( TouchDown{ .mFinger = 8, .mPosition = { 0.9f, 0.9f } } );
    input.Handle( TouchUp{ .mFinger = 7, .mPosition = { 0.1f, 0.5f } } );
    CHECK( input.LatchStep().Down( "jump"_n ) );
    input.Handle( TouchUp{ .mFinger = 8, .mPosition = { 0.9f, 0.9f } } );
    CHECK( input.LatchStep().Released( "jump"_n ) );

    // One that starts elsewhere holds nothing, even when it slides in.
    input.Handle( TouchDown{ .mFinger = 9, .mPosition = { 0.2f, 0.5f } } );
    input.Handle( TouchMove{ .mFinger = 9, .mPosition = { 0.8f, 0.5f }, .mDelta = { 0.6f, 0.0f } } );
    CHECK_FALSE( input.LatchStep().Down( "jump"_n ) );
}

TEST_CASE( "An action the map does not have is an error naming those it does" )
{
    const Name typo = *Name::TryFrom( "jupm" );
    Input input( Controls() );
    CHECK_THROWS_WITH_AS( input.LatchStep().Pressed( typo ), "no action \"jupm\" (actions: jump, move_x, fire)",
                          logic_error );
    CHECK_THROWS_WITH_AS( Actions().Pressed( "jump"_n ), "no action \"jump\" (no actions)", logic_error );
}

TEST_CASE( "An action map keeps its actions in the order first bound" )
{
    const ActionMap map = Controls();
    REQUIRE( map.List().size() == 3 );
    CHECK( map.List()[0].mName == "jump"_n );
    CHECK( map.List()[0].mBindings.size() == 3 );
    CHECK( map.List()[1].mBindings[0].mScale == -1.0f );
    CHECK( map.TryIndexOf( "fire"_n ) == 2u );
    CHECK( map.TryIndexOf( "crouch"_n ) == nullopt );
    CHECK( map.Names() == "jump, move_x, fire" );

    ActionMap wrong;
    CHECK_THROWS_AS( wrong.Bind( "Jump", Key::Space ), logic_error );
}
