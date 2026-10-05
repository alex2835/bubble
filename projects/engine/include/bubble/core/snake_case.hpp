#pragma once
#include "bubble/types/string.hpp"
#include <cctype>

namespace bubble
{
// "OuterCutOff" -> "outer_cut_off", "GPUTexture" -> "gpu_texture".
// Everything a user sees is snake_case, while C++ names are PascalCase.
inline string ToSnakeCase( string_view name )
{
    const auto upper = [&]( size_t i ) { return std::isupper( (unsigned char)name[i] ) != 0; };
    const auto lower = [&]( size_t i ) { return std::islower( (unsigned char)name[i] ) != 0; };

    string out;
    for ( size_t i = 0; i < name.size(); i++ )
    {
        // A word starts at a capital after a lowercase letter or a digit, and
        // at the last capital of an acronym that a lowercase letter follows.
        if ( i > 0 and upper( i ) and ( not upper( i - 1 ) or ( i + 1 < name.size() and lower( i + 1 ) ) ) and
             name[i - 1] != '_' )
            out += '_';
        out += (char)std::tolower( (unsigned char)name[i] );
    }
    return out;
}
}
