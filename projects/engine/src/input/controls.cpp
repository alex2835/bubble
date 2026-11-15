#include "bubble/input/controls.hpp"
#include "bubble/types/containers.hpp"

namespace bubble
{
namespace
{
struct KeyEntry
{
    Key mKey;
    string_view mName;
};

constexpr KeyEntry cKeys[] = {
    { Key::A, "a" },
    { Key::B, "b" },
    { Key::C, "c" },
    { Key::D, "d" },
    { Key::E, "e" },
    { Key::F, "f" },
    { Key::G, "g" },
    { Key::H, "h" },
    { Key::I, "i" },
    { Key::J, "j" },
    { Key::K, "k" },
    { Key::L, "l" },
    { Key::M, "m" },
    { Key::N, "n" },
    { Key::O, "o" },
    { Key::P, "p" },
    { Key::Q, "q" },
    { Key::R, "r" },
    { Key::S, "s" },
    { Key::T, "t" },
    { Key::U, "u" },
    { Key::V, "v" },
    { Key::W, "w" },
    { Key::X, "x" },
    { Key::Y, "y" },
    { Key::Z, "z" },
    { Key::Digit1, "1" },
    { Key::Digit2, "2" },
    { Key::Digit3, "3" },
    { Key::Digit4, "4" },
    { Key::Digit5, "5" },
    { Key::Digit6, "6" },
    { Key::Digit7, "7" },
    { Key::Digit8, "8" },
    { Key::Digit9, "9" },
    { Key::Digit0, "0" },
    { Key::Enter, "enter" },
    { Key::Escape, "escape" },
    { Key::Backspace, "backspace" },
    { Key::Tab, "tab" },
    { Key::Space, "space" },
    { Key::Minus, "minus" },
    { Key::Equals, "equals" },
    { Key::LeftBracket, "left_bracket" },
    { Key::RightBracket, "right_bracket" },
    { Key::Backslash, "backslash" },
    { Key::Semicolon, "semicolon" },
    { Key::Apostrophe, "apostrophe" },
    { Key::Grave, "grave" },
    { Key::Comma, "comma" },
    { Key::Period, "period" },
    { Key::Slash, "slash" },
    { Key::CapsLock, "caps_lock" },
    { Key::F1, "f1" },
    { Key::F2, "f2" },
    { Key::F3, "f3" },
    { Key::F4, "f4" },
    { Key::F5, "f5" },
    { Key::F6, "f6" },
    { Key::F7, "f7" },
    { Key::F8, "f8" },
    { Key::F9, "f9" },
    { Key::F10, "f10" },
    { Key::F11, "f11" },
    { Key::F12, "f12" },
    { Key::PrintScreen, "print_screen" },
    { Key::ScrollLock, "scroll_lock" },
    { Key::Pause, "pause" },
    { Key::Insert, "insert" },
    { Key::Home, "home" },
    { Key::PageUp, "page_up" },
    { Key::Delete, "delete" },
    { Key::End, "end" },
    { Key::PageDown, "page_down" },
    { Key::Right, "right" },
    { Key::Left, "left" },
    { Key::Down, "down" },
    { Key::Up, "up" },
    { Key::NumLock, "num_lock" },
    { Key::KeypadDivide, "keypad_divide" },
    { Key::KeypadMultiply, "keypad_multiply" },
    { Key::KeypadMinus, "keypad_minus" },
    { Key::KeypadPlus, "keypad_plus" },
    { Key::KeypadEnter, "keypad_enter" },
    { Key::Keypad1, "keypad_1" },
    { Key::Keypad2, "keypad_2" },
    { Key::Keypad3, "keypad_3" },
    { Key::Keypad4, "keypad_4" },
    { Key::Keypad5, "keypad_5" },
    { Key::Keypad6, "keypad_6" },
    { Key::Keypad7, "keypad_7" },
    { Key::Keypad8, "keypad_8" },
    { Key::Keypad9, "keypad_9" },
    { Key::Keypad0, "keypad_0" },
    { Key::KeypadPeriod, "keypad_period" },
    { Key::Menu, "menu" },
    { Key::LeftCtrl, "left_ctrl" },
    { Key::LeftShift, "left_shift" },
    { Key::LeftAlt, "left_alt" },
    { Key::LeftSuper, "left_super" },
    { Key::RightCtrl, "right_ctrl" },
    { Key::RightShift, "right_shift" },
    { Key::RightAlt, "right_alt" },
    { Key::RightSuper, "right_super" },
};

static_assert( static_cast<u32>( Key::RightSuper ) + 1 == cKeySlots );

constexpr array<string_view, cMouseButtons> cMouseButtonNames = { "left", "middle", "right", "x1", "x2" };

constexpr array<string_view, cGamepadButtons> cGamepadButtonNames = {
    "south",       "east",          "west",           "north",   "back",      "guide",     "start",      "left_stick",
    "right_stick", "left_shoulder", "right_shoulder", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
};

constexpr array<string_view, cGamepadAxes> cGamepadAxisNames = { "left_x",  "left_y",       "right_x",
                                                                 "right_y", "left_trigger", "right_trigger" };
}

string_view KeyName( Key key )
{
    for ( const KeyEntry& entry : cKeys )
        if ( entry.mKey == key )
            return entry.mName;
    return {};
}

opt<Key> TryKeyFromCode( u32 code )
{
    for ( const KeyEntry& entry : cKeys )
        if ( static_cast<u32>( entry.mKey ) == code )
            return entry.mKey;
    return nullopt;
}

opt<Key> TryKeyFromName( string_view name )
{
    for ( const KeyEntry& entry : cKeys )
        if ( entry.mName == name )
            return entry.mKey;
    return nullopt;
}

string_view MouseButtonName( MouseButton button )
{
    return cMouseButtonNames.at( static_cast<size_t>( button ) );
}

string_view GamepadButtonName( GamepadButton button )
{
    return cGamepadButtonNames.at( static_cast<size_t>( button ) );
}

string_view GamepadAxisName( GamepadAxis axis )
{
    return cGamepadAxisNames.at( static_cast<size_t>( axis ) );
}
}
