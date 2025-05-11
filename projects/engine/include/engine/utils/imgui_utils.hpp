#pragma once
#include <imgui.h>
#include "engine/types/string.hpp"

namespace ImGui
{
inline void InputText( std::string_view label, std::string& str )
{
    char buffer[128] = { 0 };
    str.copy( buffer, sizeof( buffer ) );
    ImGui::InputText( label.data(), buffer, sizeof( buffer ) );
    str.assign( buffer );
}

}
