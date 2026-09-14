#pragma once
#include <imgui.h>
#include "engine/types/string.hpp"

namespace ImGui
{
// True when the text was edited this frame, like the char* form.
inline bool InputText( std::string_view label, std::string& str )
{
    char buffer[128] = { 0 };
    str.copy( buffer, sizeof( buffer ) - 1 );
    const bool changed = ImGui::InputText( label.data(), buffer, sizeof( buffer ) );
    str.assign( buffer );
    return changed;
}

}
