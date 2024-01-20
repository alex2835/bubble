#pragma once
#include <vector>
#include "engine/utils/types.hpp"
#include "engine/window/input.hpp"

namespace bubble
{

MouseInput::MouseInput()
{
    for ( size_t key = 0; key < MAX_MOUSE_KEYS_SIZE; key++ )
        mKeyState[key] = NO_STATE;
}   

void MouseInput::OnUpdate()
{
    mKeyMods = KeyMods();
}

bool MouseInput::IsKeyCliked( MouseKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::Press;
}

i32 MouseInput::IsKeyPressed( MouseKey key ) const 
{
    return mKeyState[(i32)key] != NO_STATE;
}



KeyboardInput::KeyboardInput()
{
    for ( size_t key = 0; key < MAX_KEYBOAR_KEYS_SIZE; key++ )
        mKeyState[key] = NO_STATE;
}

void KeyboardInput::OnUpdate()
{
    mKeyMods = KeyMods();
}

bool KeyboardInput::IsKeyCliked( KeyboardKey key ) const
{
    return mKeyState[(i32)key] == (i32)KeyAction::Press;
}

i32 KeyboardInput::IsKeyPressed( KeyboardKey key ) const
{
    return mKeyState[(i32)key] != NO_STATE;
}



}