#pragma once
#include <string>
#include <stdexcept>
#include "window/key_map.hpp"
#include "serialization/event_serialization.hpp"


namespace bubble
{

template <>
std::string ToString<KeyboardKey>( const KeyboardKey& key )
{
    switch( key )
    {
    case KeyboardKey::UNKNOWN:
        return "KEY_UNKNOWN";
    case KeyboardKey::SPACE:
        return "KEY_SPACE";
    case KeyboardKey::APOSTROPHE:
        return "KEY_APOSTROPHE";
    case KeyboardKey::COMMA:
        return "KEY_COMMA";
    case KeyboardKey::MINUS:
        return "KEY_MINUS";
    case KeyboardKey::PERIOD:
        return "KEY_PERIOD";
    case KeyboardKey::SLASH:
        return "KEY_SLASH";
    case KeyboardKey::ZERO:
        return "KEY_0";
    case KeyboardKey::ONE:
        return "KEY_1";
    case KeyboardKey::TWO:
        return "KEY_2";
    case KeyboardKey::THREE:
        return "KEY_3";
    case KeyboardKey::FOUR:
        return "KEY_4";
    case KeyboardKey::FIVE:
        return "KEY_5";
    case KeyboardKey::SIX:
        return "KEY_6";
    case KeyboardKey::SEVEN:
        return "KEY_7";
    case KeyboardKey::EIGHT:
        return "KEY_8";
    case KeyboardKey::NINE:
        return "KEY_9";
    case KeyboardKey::SEMICOLON:
        return "KEY_SEMICOLON";
    case KeyboardKey::EQUAL:
        return "KEY_EQUAL";
    case KeyboardKey::A:
        return "KEY_A";
    case KeyboardKey::B:
        return "KEY_B";
    case KeyboardKey::C:
        return "KEY_C";
    case KeyboardKey::D:
        return "KEY_D";
    case KeyboardKey::E:
        return "KEY_E";
    case KeyboardKey::F:
        return "KEY_F";
    case KeyboardKey::G:
        return "KEY_G";
    case KeyboardKey::H:
        return "KEY_H";
    case KeyboardKey::I:
        return "KEY_I";
    case KeyboardKey::J:
        return "KEY_J";
    case KeyboardKey::K:
        return "KEY_K";
    case KeyboardKey::L:
        return "KEY_L";
    case KeyboardKey::M:
        return "KEY_M";
    case KeyboardKey::N:
        return "KEY_N";
    case KeyboardKey::O:
        return "KEY_O";
    case KeyboardKey::P:
        return "KEY_P";
    case KeyboardKey::Q:
        return "KEY_Q";
    case KeyboardKey::R:
        return "KEY_R";
    case KeyboardKey::S:
        return "KEY_S";
    case KeyboardKey::T:
        return "KEY_T";
    case KeyboardKey::U:
        return "KEY_U";
    case KeyboardKey::V:
        return "KEY_V";
    case KeyboardKey::W:
        return "KEY_W";
    case KeyboardKey::X:
        return "KEY_X";
    case KeyboardKey::Y:
        return "KEY_Y";
    case KeyboardKey::Z:
        return "KEY_Z";
    case KeyboardKey::LEFT_BRACKET:
        return "KEY_LEFT_BRACKET";
    case KeyboardKey::BACKSLASH:
        return "KEY_BACKSLASH";
    case KeyboardKey::RIGHT_BRACKET:
        return "KEY_RIGHT_BRACKET";
    case KeyboardKey::GRAVE_ACCENT:
        return "KEY_GRAVE_ACCENT";
    case KeyboardKey::WORLD_1:
        return "KEY_WORLD_1";
    case KeyboardKey::WORLD_2:
        return "KEY_WORLD_2";
    case KeyboardKey::ESCAPE:
        return "KEY_ESCAPE";
    case KeyboardKey::ENTER:
        return "KEY_ENTER";
    case KeyboardKey::TAB:
        return "KEY_TAB";
    case KeyboardKey::BACKSPACE:
        return "KEY_BACKSPACE";
    case KeyboardKey::INSERT:
        return "KEY_INSERT";
    case KeyboardKey::DELETE:
        return "KEY_DELETE";
    case KeyboardKey::RIGHT:
        return "KEY_RIGHT";
    case KeyboardKey::LEFT:
        return "KEY_LEFT";
    case KeyboardKey::DOWN:
        return "KEY_DOWN";
    case KeyboardKey::UP:
        return "KEY_UP";
    case KeyboardKey::PAGE_UP:
        return "KEY_PAGE_UP";
    case KeyboardKey::PAGE_DOWN:
        return "KEY_PAGE_DOWN";
    case KeyboardKey::HOME:
        return "KEY_HOME";
    case KeyboardKey::END:
        return "KEY_END";
    case KeyboardKey::CAPS_LOCK:
        return "KEY_CAPS_LOCK";
    case KeyboardKey::SCROLL_LOCK:
        return "KEY_SCROLL_LOCK";
    case KeyboardKey::NUM_LOCK:
        return "KEY_NUM_LOCK";
    case KeyboardKey::PRINT_SCREEN:
        return "KEY_PRINT_SCREEN";
    case KeyboardKey::PAUSE:
        return "KEY_PAUSE";
    case KeyboardKey::F1:
        return "KEY_F1";
    case KeyboardKey::F2:
        return "KEY_F2";
    case KeyboardKey::F3:
        return "KEY_F3";
    case KeyboardKey::F4:
        return "KEY_F4";
    case KeyboardKey::F5:
        return "KEY_F5";
    case KeyboardKey::F6:
        return "KEY_F6";
    case KeyboardKey::F7:
        return "KEY_F7";
    case KeyboardKey::F8:
        return "KEY_F8";
    case KeyboardKey::F9:
        return "KEY_F9";
    case KeyboardKey::F10:
        return "KEY_F10";
    case KeyboardKey::F11:
        return "KEY_F11";
    case KeyboardKey::F12:
        return "KEY_F12";
    case KeyboardKey::F13:
        return "KEY_F13";
    case KeyboardKey::F14:
        return "KEY_F14";
    case KeyboardKey::F15:
        return "KEY_F15";
    case KeyboardKey::F16:
        return "KEY_F16";
    case KeyboardKey::F17:
        return "KEY_F17";
    case KeyboardKey::F18:
        return "KEY_F18";
    case KeyboardKey::F19:
        return "KEY_F19";
    case KeyboardKey::F20:
        return "KEY_F20";
    case KeyboardKey::F21:
        return "KEY_F21";
    case KeyboardKey::F22:
        return "KEY_F22";
    case KeyboardKey::F23:
        return "KEY_F23";
    case KeyboardKey::F24:
        return "KEY_F24";
    case KeyboardKey::F25:
        return "KEY_F25";
    case KeyboardKey::KP_0:
        return "KEY_KP_0";
    case KeyboardKey::KP_1:
        return "KEY_KP_1";
    case KeyboardKey::KP_2:
        return "KEY_KP_2";
    case KeyboardKey::KP_3:
        return "KEY_KP_3";
    case KeyboardKey::KP_4:
        return "KEY_KP_4";
    case KeyboardKey::KP_5:
        return "KEY_KP_5";
    case KeyboardKey::KP_6:
        return "KEY_KP_6";
    case KeyboardKey::KP_7:
        return "KEY_KP_7";
    case KeyboardKey::KP_8:
        return "KEY_KP_8";
    case KeyboardKey::KP_9:
        return "KEY_KP_9";
    case KeyboardKey::KP_DECIMAL:
        return "KEY_KP_DECIMAL";
    case KeyboardKey::KP_DIVIDE:
        return "KEY_KP_DIVIDE";
    case KeyboardKey::KP_MULTIPLY:
        return "KEY_KP_MULTIPLY";
    case KeyboardKey::KP_SUBTRACT:
        return "KEY_KP_SUBTRACT";
    case KeyboardKey::KP_ADD:
        return "KEY_KP_ADD";
    case KeyboardKey::KP_ENTER:
        return "KEY_KP_ENTER";
    case KeyboardKey::KP_EQUAL:
        return "KEY_KP_EQUAL";
    case KeyboardKey::LEFT_SHIFT:
        return "KEY_LEFT_SHIFT";
    case KeyboardKey::LEFT_CONTROL:
        return "KEY_LEFT_CONTROL";
    case KeyboardKey::LEFT_ALT:
        return "KEY_LEFT_ALT";
    case KeyboardKey::LEFT_SUPER:
        return "KEY_LEFT_SUPER";
    case KeyboardKey::RIGHT_SHIFT:
        return "KEY_RIGHT_SHIFT";
    case KeyboardKey::RIGHT_CONTROL:
        return "KEY_RIGHT_CONTROL";
    case KeyboardKey::RIGHT_ALT:
        return "KEY_RIGHT_ALT";
    case KeyboardKey::RIGHT_SUPER:
        return "KEY_RIGHT_SUPER";
    case KeyboardKey::MENU:
        return "KEY_MENU";
    default:
        throw std::runtime_error( "Unknown keyboard key: " + std::to_string( static_cast<int>( key ) ) );
    }
}

template <>
std::string ToString<KeyAction>( const KeyAction& action )
{
    switch( action )
    {
    case KeyAction::Release:
        return "Release";
    case KeyAction::Press:
        return "Press";
    case KeyAction::Repeat:
        return "Repeat";
    default:
        throw std::runtime_error( "Invalid KeyAction " + std::to_string( static_cast<int>( action ) ) );
    }
}


template <> 
std::string ToString<KeyMods>( const KeyMods& mods )
{
    std::string res;
    res = res + "SHIFT: " + ( mods.SHIFT ? "true" : "false" );
    res = res + " CONTROL: " + ( mods.CONTROL ? "true" : "false" );
    res = res + " ALT: " + ( mods.ALT ? "true" : "false" );
    res = res + " SUPER: " + ( mods.SUPER ? "true" : "false" );
    res = res + " CAPS_LOCK: " + ( mods.CAPS_LOCK ? "true" : "false" );
    res = res + " NUM_LOCK: " + ( mods.NUM_LOCK ? "true" : "false" );
    return res;
}

template <>
std::string ToString<MouseKey>( const MouseKey& key )
{
    switch( key )
    {
    case MouseKey::UNKNOWN:
        return "MOUSE_UNKNOWN";
    case MouseKey::BUTTON_LEFT:
        return "MOUSE_BUTTON_LEFT";
    case MouseKey::BUTTON_RIGHT:
        return "MOUSE_BUTTON_RIGHT";
    case MouseKey::BUTTON_MIDDLE:
        return "MOUSE_BUTTON_MIDDLE";
    case MouseKey::BUTTON_4:
        return "MOUSE_BUTTON_4";
    case MouseKey::BUTTON_5:
        return "MOUSE_BUTTON_5";
    case MouseKey::BUTTON_6:
        return "MOUSE_BUTTON_6";
    case MouseKey::BUTTON_7:
        return "MOUSE_BUTTON_7";
    case MouseKey::BUTTON_8:
        return "MOUSE_BUTTON_8";
    default:
        throw std::runtime_error( "Unknown mouse key: " + std::to_string( static_cast<int>( key ) ) );
    }
}

template <>
std::string ToString<Event>( const Event& key )
{
    std::string res;
    if( key.mType == EventType::KeyboardKey )
    {
        res += "Type: KeyboardKey";
        res = res + "\nKey: " + ToString( key.mKeyboard.Key );
        res = res + "\nAction: " + ToString( key.mKeyboard.Action );
        res = res + "\nMods: " + ToString( key.mKeyboard.Mods );
    }
    else if( key.mType == EventType::MouseKey )
    {
        res += "Type: MouseKey";
        res = res + "\nKey: " + ToString( key.mMouse.Key );
        res = res + "\nAction: " + ToString( key.mMouse.Action );
        res = res + "\nMods: " + ToString( key.mMouse.Mods );
    }
    else if( key.mType == EventType::MouseMove )
    {
        res += "Type: MouseMove";
        res = res + "\nPosition: x:" + std::to_string( key.mMouse.Pos.x ) + 
                             " y:" + std::to_string( key.mMouse.Pos.x );
        res = res + "\nOffset: x:" + std::to_string( key.mMouse.Offset.x ) +
                           " y:" + std::to_string( key.mMouse.Offset.x );
    }
    else if( key.mType == EventType::MouseZoom )
    {
        res += "Type: MouseZoom";
        res += "\nZoomOffset: " + std::to_string( key.mMouse.ZoomOffset );
    }
    return res;
}

}