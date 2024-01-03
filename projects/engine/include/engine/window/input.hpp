#pragma once 
#include <glm/glm.hpp>
#include "engine/utils/imexp.hpp"
#include "engine/window/key_map.hpp"

namespace bubble
{
constexpr int NO_STATE = -1;
constexpr int MAX_KEYBOAR_KEYS_SIZE = 350;
constexpr int MAX_MOUSE_KEYS_SIZE = 8;

struct BUBBLE_ENGINE_EXPORT MouseInput
{
    glm::vec2 mMousePos;
    glm::vec2 mMouseOffset;
    int mKeyState[MAX_MOUSE_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    MouseInput();
    void OnUpdate();
    bool IsKeyCliked( MouseKey key ) const;
    int IsKeyPressed( MouseKey key ) const;
};


struct BUBBLE_ENGINE_EXPORT KeyboardInput
{
    int mKeyState[MAX_KEYBOAR_KEYS_SIZE] = { 0 };
    KeyMods mKeyMods;

    KeyboardInput();
    void OnUpdate();
    bool IsKeyCliked( KeyboardKey key ) const;
    int IsKeyPressed( KeyboardKey key ) const;
};

}