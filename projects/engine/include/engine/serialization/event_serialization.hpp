#pragma once
#include "engine/types/string.hpp"
#include "engine/window/event.hpp"

namespace bubble
{

template <typename T>
string ToString( const T& ) = delete;

template <>
string ToString<KeyboardKey>( const KeyboardKey& key );

template <>
string ToString<KeyAction>( const KeyAction& action );

template <>
string ToString<KeyMods>( const KeyMods& mod );

template <>
string ToString<MouseKey>( const MouseKey& key );

template <>
string ToString<Event>( const Event& key );

}