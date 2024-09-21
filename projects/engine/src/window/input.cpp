#pragma once
#include "engine/types/number.hpp"
#include "engine/types/array.hpp"
#include "engine/window/input.hpp"

namespace bubble
{
MouseInput::MouseInput()
{
    for ( u64 key = 0; key < MAX_MOUSE_KEYS_SIZE; key++ )
        mKeyState[key] = NO_STATE;
}   

void MouseInput::OnUpdate()
{
    mMouseOffset = vec2();
}

bool MouseInput::IsKeyCliked( MouseKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::PRESS;
}

bool MouseInput::IsKeyPressed( MouseKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::PRESS or
           mKeyState[(i32)key] == (i32)KeyAction::REPEAT;
}



KeyboardInput::KeyboardInput()
{
    for ( u64 key = 0; key < MAX_KEYBOAR_KEYS_SIZE; key++ )
        mKeyState[key] = NO_STATE;
}

void KeyboardInput::OnUpdate()
{
}

bool KeyboardInput::IsKeyCliked( KeyboardKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::PRESS;
}

bool KeyboardInput::IsKeyPressed( KeyboardKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::PRESS or
           mKeyState[(i32)key] == (i32)KeyAction::REPEAT;
}

}