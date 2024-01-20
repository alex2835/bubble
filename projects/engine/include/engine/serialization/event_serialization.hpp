#pragma once
#include <string>
#include "engine/utils/imexp.hpp"
#include "engine/window/key_map.hpp"
#include "engine/window/event.hpp"


namespace bubble
{

template <typename T>
string ToString( const T& ) = delete;

template <>
BUBBLE_ENGINE_EXPORT string ToString<KeyboardKey>( const KeyboardKey& key );

template <>
BUBBLE_ENGINE_EXPORT string ToString<KeyAction>( const KeyAction& action );

template <>
BUBBLE_ENGINE_EXPORT string ToString<KeyMods>( const KeyMods& mod );

template <>
BUBBLE_ENGINE_EXPORT string ToString<MouseKey>( const MouseKey& key );

template <>
BUBBLE_ENGINE_EXPORT string ToString<Event>( const Event& key );

}