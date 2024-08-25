#pragma once
#include <imgui.h>
#include "engine/types/string.hpp"

namespace ImGui
{
inline void InputText( bubble::string& str )
{
    char buffer[64] = { 0 };
    str.copy( buffer, sizeof( buffer ) );
    ImGui::InputText( "##Tag", buffer, sizeof( buffer ) );
    str.assign( buffer );
}

}
