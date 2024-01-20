#pragma once
#include "engine/window/key_map.hpp"
#include "engine/window/input.hpp"

namespace bubble
{

enum class EventType
{
    KeyboardKey,
    MouseKey,
    MouseMove,
    MouseZoom,

    ShouldClose
};

struct Event
{
    EventType mType = EventType::KeyboardKey;
    struct
    {
        KeyboardKey Key = KeyboardKey::UNKNOWN;
        KeyAction Action = KeyAction::Release;
        KeyMods Mods;
    } mKeyboard;
    struct
    {
        MouseKey Key = MouseKey::UNKNOWN;
        KeyAction Action = KeyAction::Release;
        KeyMods Mods;
        vec2 Pos;
        vec2 Offset;
        f32 ZoomOffset;
    } mMouse;

    const KeyboardInput* mKeyboardInput;
    const MouseInput* mMouseInput;
};

}