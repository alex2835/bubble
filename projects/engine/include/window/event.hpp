#pragma once
#include <glm/glm.hpp>
#include "window/key_map.hpp"

namespace bubble
{

enum class EventType
{
    KeyboardKey,
    MouseKey,
    MouseMove,
    MouseZoom,

};

enum class KeyAction
{
    Release = GLFW_RELEASE,
    Press   = GLFW_PRESS,
    Repeat  = GLFW_REPEAT
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
        glm::fvec2 Pos = { 0, 0 };
        glm::fvec2 Offset = { 0, 0 };
        float ZoomOffset = 0;
    } mMouse;
};

}