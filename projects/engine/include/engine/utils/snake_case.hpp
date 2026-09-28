#pragma once
#include "engine/types/string.hpp"
#include <cctype>

namespace bubble
{
// "OuterCutOff" -> "outer_cut_off". The Lua API and reflected names are
// snake_case throughout, while C++ names are PascalCase.
inline string ToSnakeCase( string_view name )
{
    string out;
    for ( size_t i = 0; i < name.size(); i++ )
    {
        const unsigned char c = (unsigned char)name[i];
        if ( std::isupper( c ) and i > 0 and not std::isupper( (unsigned char)name[i - 1] ) )
            out += '_';
        out += (char)std::tolower( c );
    }
    return out;
}

}
