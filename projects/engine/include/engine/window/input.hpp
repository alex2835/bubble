#pragma once 
#include "engine/types/glm.hpp"
#include "engine/types/number.hpp"
#include "engine/window/key_map.hpp"

namespace bubble
{
constexpr i32 NO_STATE = -1;
constexpr i32 MAX_KEYBOAR_KEYS_SIZE = 350;
constexpr i32 MAX_MOUSE_KEYS_SIZE = 8;

struct MouseInput
{
    vec2 mMousePos;
    vec2 mMouseOffset;
    i32 mKeyState[MAX_MOUSE_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    MouseInput();
    void OnUpdate();
    bool IsKeyCliked( MouseKey key ) const;
    bool IsKeyPressed( MouseKey key ) const;
};


struct KeyboardInput
{
    i32 mKeyState[MAX_KEYBOAR_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    KeyboardInput();
    void OnUpdate();
    bool IsKeyCliked( KeyboardKey key ) const;
    bool IsKeyPressed( KeyboardKey key ) const;
};

class WindowInput
{
public:
    bool IsKeyCliked( KeyboardKey key ) const { return mKeyboardInput.IsKeyCliked( key ); }
    bool IsKeyPressed( KeyboardKey key ) const { return mKeyboardInput.IsKeyPressed( key ); }
    KeyMods KeyMods() const { return mKeyboardInput.mKeyMods; }

    bool IsKeyCliked( MouseKey key ) const { return mMouseInput.IsKeyCliked( key ); }
    bool IsKeyPressed( MouseKey key ) const { return mMouseInput.IsKeyPressed( key ); }
    vec2 MouseOffset() const { return mMouseInput.mMouseOffset; }
    vec2 MousePos() const { return mMouseInput.mMousePos; }

protected:
    KeyboardInput mKeyboardInput;
    MouseInput mMouseInput;
    friend class Window;
};

}