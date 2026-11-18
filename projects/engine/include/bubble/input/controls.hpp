#pragma once
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// A key by where it is on the keyboard, not by what it types: WASD is WASD
// on any layout. The values are the USB HID keyboard usages, which most
// platforms - SDL among them - give scancodes in.
enum class Key : u16
{
    Unknown = 0,
    A = 4,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Digit1 = 30,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Digit0,
    Enter = 40,
    Escape,
    Backspace,
    Tab,
    Space,
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon = 51,
    Apostrophe,
    Grave,
    Comma,
    Period,
    Slash,
    CapsLock,
    F1 = 58,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    PrintScreen = 70,
    ScrollLock,
    Pause,
    Insert,
    Home,
    PageUp,
    Delete,
    End,
    PageDown,
    Right,
    Left,
    Down,
    Up,
    NumLock = 83,
    KeypadDivide,
    KeypadMultiply,
    KeypadMinus,
    KeypadPlus,
    KeypadEnter,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    Keypad0,
    KeypadPeriod,
    Menu = 101,
    LeftCtrl = 224,
    LeftShift,
    LeftAlt,
    LeftSuper,
    RightCtrl,
    RightShift,
    RightAlt,
    RightSuper,
};
// One past the largest Key: a table indexed by keys has this many slots.
constexpr u32 cKeySlots = 232;

enum class MouseButton : u8
{
    Left,
    Middle,
    Right,
    X1,
    X2,
};
constexpr u32 cMouseButtons = 5;

// How far the mouse went: across and down in the window's units, the wheel
// in notches - up positive.
enum class MouseAxis : u8
{
    X,
    Y,
    Wheel,
};
constexpr u32 cMouseAxes = 3;

// Named by where the button is, as on every pad: south is A on an Xbox
// pad and the cross on a PlayStation one.
enum class GamepadButton : u8
{
    South,
    East,
    West,
    North,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
};
constexpr u32 cGamepadButtons = 15;

// Sticks go from -1 to 1, right and down positive; triggers from 0 to 1.
enum class GamepadAxis : u8
{
    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
};
constexpr u32 cGamepadAxes = 6;

// The names a player and a project's files see, in snake_case: "space",
// "left_shift", "keypad_1", "1". Unknown has none.
string_view KeyName( Key key );
// A key by its HID usage - a scancode - when it is one of Key's.
opt<Key> TryKeyFromCode( u32 code );
opt<Key> TryKeyFromName( string_view name );

string_view MouseButtonName( MouseButton button );
string_view MouseAxisName( MouseAxis axis );
string_view GamepadButtonName( GamepadButton button );
string_view GamepadAxisName( GamepadAxis axis );
opt<MouseButton> TryMouseButtonFromName( string_view name );
opt<MouseAxis> TryMouseAxisFromName( string_view name );
opt<GamepadButton> TryGamepadButtonFromName( string_view name );
opt<GamepadAxis> TryGamepadAxisFromName( string_view name );
}
