#include "engine/pch/pch.hpp"
#include "engine/utils/lookalike_text.hpp"

namespace bubble
{
namespace
{
// The text as its letters look: ASCII lowercased, and the Cyrillic letters
// that are drawn like Latin ones taken as those.
string Skeleton( string_view text )
{
    // By code point: the sources are not compiled as UTF-8.
    static const std::pair<char32_t, char> lookalikes[] = {
        // a        b             e             yo            k             m             n (drawn h)
        { 0x430, 'a' }, { 0x432, 'b' }, { 0x435, 'e' }, { 0x451, 'e' }, { 0x43A, 'k' }, { 0x43C, 'm' }, { 0x43D, 'h' },
        // o        r (drawn p)   s (drawn c)   t             u (drawn y)   h (drawn x)   i
        { 0x43E, 'o' }, { 0x440, 'p' }, { 0x441, 'c' }, { 0x442, 't' }, { 0x443, 'y' }, { 0x445, 'x' }, { 0x456, 'i' },
        // The same, capital.
        { 0x410, 'a' }, { 0x412, 'b' }, { 0x415, 'e' }, { 0x41A, 'k' }, { 0x41C, 'm' }, { 0x41D, 'h' }, { 0x41E, 'o' },
        { 0x420, 'p' }, { 0x421, 'c' }, { 0x422, 't' }, { 0x423, 'y' }, { 0x425, 'x' }, { 0x406, 'i' },
    };
    string out;
    for ( size_t i = 0; i < text.size(); )
    {
        const unsigned char c = text[i];
        // Two byte UTF-8 is all Cyrillic needs; anything else passes as is.
        if ( ( c & 0xE0 ) == 0xC0 and i + 1 < text.size() )
        {
            const char32_t code = ( char32_t( c & 0x1F ) << 6 ) | char32_t( text[i + 1] & 0x3F );
            const auto it = std::ranges::find( lookalikes, code, &std::pair<char32_t, char>::first );
            if ( it != std::end( lookalikes ) )
                out += it->second;
            else
                out += text.substr( i, 2 );
            i += 2;
            continue;
        }
        out += (char)std::tolower( c );
        i++;
    }
    return out;
}
}

bool LooksAlike( string_view a, string_view b )
{
    return Skeleton( a ) == Skeleton( b );
}

string NonAsciiLetters( string_view text )
{
    string out;
    for ( size_t i = 0; i < text.size(); )
    {
        const unsigned char c = text[i];
        size_t length = c < 0x80 ? 1 : ( c & 0xE0 ) == 0xC0 ? 2 : ( c & 0xF0 ) == 0xE0 ? 3 : 4;
        length = std::min( length, text.size() - i );
        if ( length > 1 )
        {
            char32_t code = c & ( 0xFF >> ( length + 1 ) );
            for ( size_t k = 1; k < length; k++ )
                code = ( code << 6 ) | char32_t( text[i + k] & 0x3F );
            out += std::format( "{}'{}' (U+{:04X})", out.empty() ? "" : ", ", text.substr( i, length ), (u32)code );
        }
        i += length;
    }
    return out;
}

}
