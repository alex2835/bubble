#pragma once
#include "bubble/types/string.hpp"

namespace bubble
{
// "OuterCutOff" -> "outer_cut_off", "GPUTexture" -> "gpu_texture".
// Everything a user sees is snake_case, while C++ names are PascalCase.
inline string ToSnakeCase( string_view name )
{
    // Identifiers are ASCII; <cctype> would also follow the C locale.
    const auto upper = [&]( size_t i ) { return name[i] >= 'A' and name[i] <= 'Z'; };
    const auto lower = [&]( size_t i ) { return name[i] >= 'a' and name[i] <= 'z'; };

    string out;
    for ( size_t i = 0; i < name.size(); i++ )
    {
        // A word starts at a capital after a lowercase letter or a digit, and
        // at the last capital of an acronym that a lowercase letter follows.
        if ( i > 0 and upper( i ) and ( not upper( i - 1 ) or ( i + 1 < name.size() and lower( i + 1 ) ) ) and
             name[i - 1] != '_' )
            out += '_';
        out += upper( i ) ? char( name[i] - 'A' + 'a' ) : name[i];
    }
    return out;
}
}
