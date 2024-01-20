#pragma once 
#include "engine/utils/imexp.hpp"
#include "engine/window/key_map.hpp"
#include "engine/utils/types.hpp"

namespace bubble
{
constexpr i32 NO_STATE = -1;
constexpr i32 MAX_KEYBOAR_KEYS_SIZE = 350;
constexpr i32 MAX_MOUSE_KEYS_SIZE = 8;

struct BUBBLE_ENGINE_EXPORT MouseInput
{
    vec2 mMousePos;
    vec2 mMouseOffset;
    i32 mKeyState[MAX_MOUSE_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    MouseInput();
    void OnUpdate();
    bool IsKeyCliked( MouseKey key ) const;
    i32 IsKeyPressed( MouseKey key ) const;
};


struct BUBBLE_ENGINE_EXPORT KeyboardInput
{
    i32 mKeyState[MAX_KEYBOAR_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    KeyboardInput();
    void OnUpdate();
    bool IsKeyCliked( KeyboardKey key ) const;
    i32 IsKeyPressed( KeyboardKey key ) const;
};

}