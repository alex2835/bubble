#pragma once
#include <string>
#include "utils/imexp.hpp"
#include "window/key_map.hpp"
#include "window/event.hpp"


namespace bubble
{

template <typename T>
std::string ToString( const T& ) = delete;

template <>
BUBBLE_ENGINE_DLL_EXPORT std::string ToString<KeyboardKey>( const KeyboardKey& key );

template <>
BUBBLE_ENGINE_DLL_EXPORT std::string ToString<KeyAction>( const KeyAction& action );

template <>
BUBBLE_ENGINE_DLL_EXPORT std::string ToString<KeyMods>( const KeyMods& mod );

template <>
BUBBLE_ENGINE_DLL_EXPORT std::string ToString<MouseKey>( const MouseKey& key );

template <>
BUBBLE_ENGINE_DLL_EXPORT std::string ToString<Event>( const Event& key );

}