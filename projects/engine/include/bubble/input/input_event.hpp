#pragma once
#include "bubble/input/controls.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/variant.hpp"

namespace bubble
{
// What the player did, in the engine's own terms: the platform makes these
// of its events, and a test or a replay makes them by hand.
//
// Mouse positions are in the window's own coordinates from its top left
// corner, those of Window::Size. Touch positions are shares of the window:
// (0, 0) top left, (1, 1) bottom right - what an on-screen button's area is
// given in.

struct KeyDown
{
    Key mKey = Key::Unknown;
    // Held long enough for the system to type it again.
    bool mRepeat = false;
    friend bool operator==( const KeyDown&, const KeyDown& ) = default;
};

struct KeyUp
{
    Key mKey = Key::Unknown;
    friend bool operator==( const KeyUp&, const KeyUp& ) = default;
};

// Text typed, in UTF-8: a character, or what an input method composed.
// Comes beside the keys, and only while the window takes text
// (Window::SetTextInput).
struct TextInput
{
    string mText;
    friend bool operator==( const TextInput&, const TextInput& ) = default;
};

struct MouseMove
{
    vec2 mPosition{ 0.0f };
    vec2 mDelta{ 0.0f };
    friend bool operator==( const MouseMove&, const MouseMove& ) = default;
};

struct MouseDown
{
    MouseButton mButton = MouseButton::Left;
    vec2 mPosition{ 0.0f };
    friend bool operator==( const MouseDown&, const MouseDown& ) = default;
};

struct MouseUp
{
    MouseButton mButton = MouseButton::Left;
    vec2 mPosition{ 0.0f };
    friend bool operator==( const MouseUp&, const MouseUp& ) = default;
};

// In notches of a wheel: up and right positive, whatever way the system
// scrolls.
struct MouseWheel
{
    vec2 mDelta{ 0.0f };
    friend bool operator==( const MouseWheel&, const MouseWheel& ) = default;
};

// A finger by an id that lasts from its TouchDown to its TouchUp. Touches
// have no hover: a finger is there or it is not.
struct TouchDown
{
    u64 mFinger = 0;
    vec2 mPosition{ 0.0f };
    friend bool operator==( const TouchDown&, const TouchDown& ) = default;
};

struct TouchMove
{
    u64 mFinger = 0;
    vec2 mPosition{ 0.0f };
    vec2 mDelta{ 0.0f };
    friend bool operator==( const TouchMove&, const TouchMove& ) = default;
};

// Lifted, or taken away by the system - a call coming in.
struct TouchUp
{
    u64 mFinger = 0;
    vec2 mPosition{ 0.0f };
    friend bool operator==( const TouchUp&, const TouchUp& ) = default;
};

// A gamepad by an id that lasts while it is connected.
struct GamepadAdded
{
    u32 mGamepad = 0;
    friend bool operator==( const GamepadAdded&, const GamepadAdded& ) = default;
};

struct GamepadRemoved
{
    u32 mGamepad = 0;
    friend bool operator==( const GamepadRemoved&, const GamepadRemoved& ) = default;
};

struct GamepadButtonDown
{
    u32 mGamepad = 0;
    GamepadButton mButton = GamepadButton::South;
    friend bool operator==( const GamepadButtonDown&, const GamepadButtonDown& ) = default;
};

struct GamepadButtonUp
{
    u32 mGamepad = 0;
    GamepadButton mButton = GamepadButton::South;
    friend bool operator==( const GamepadButtonUp&, const GamepadButtonUp& ) = default;
};

// Where an axis is now: sticks -1 to 1, right and down positive; triggers
// 0 to 1.
struct GamepadAxisMove
{
    u32 mGamepad = 0;
    GamepadAxis mAxis = GamepadAxis::LeftX;
    f32 mValue = 0.0f;
    friend bool operator==( const GamepadAxisMove&, const GamepadAxisMove& ) = default;
};

using InputEvent = variant<KeyDown,
                           KeyUp,
                           TextInput,
                           MouseMove,
                           MouseDown,
                           MouseUp,
                           MouseWheel,
                           TouchDown,
                           TouchMove,
                           TouchUp,
                           GamepadAdded,
                           GamepadRemoved,
                           GamepadButtonDown,
                           GamepadButtonUp,
                           GamepadAxisMove>;
}
