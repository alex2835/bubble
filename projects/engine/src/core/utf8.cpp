#include "bubble/core/utf8.hpp"

namespace bubble
{
namespace
{
bool Continuation( unsigned char b )
{
    return ( b & 0xC0 ) == 0x80;
}

// The code point at `pos` and its length in bytes; length 0 when the
// sequence there is ill-formed.
pair<char32_t, size_t> Decode( string_view text, size_t pos )
{
    const auto byte = [&]( size_t i ) { return (unsigned char)text[pos + i]; };
    const size_t left = text.size() - pos;
    const unsigned char b0 = byte( 0 );

    if ( b0 < 0x80 )
        return { b0, 1 };

    size_t length = 0;
    char32_t c = 0;
    char32_t min = 0;
    if ( ( b0 & 0xE0 ) == 0xC0 )
        length = 2, c = b0 & 0x1F, min = 0x80;
    else if ( ( b0 & 0xF0 ) == 0xE0 )
        length = 3, c = b0 & 0x0F, min = 0x800;
    else if ( ( b0 & 0xF8 ) == 0xF0 )
        length = 4, c = b0 & 0x07, min = 0x10000;
    else
        return { 0, 0 };

    if ( left < length )
        return { 0, 0 };
    for ( size_t i = 1; i < length; i++ )
    {
        if ( not Continuation( byte( i ) ) )
            return { 0, 0 };
        c = ( c << 6 ) | ( byte( i ) & 0x3F );
    }
    // Overlong forms, UTF-16 surrogates and values past Unicode are not
    // UTF-8 even when the bits line up.
    if ( c < min or ( c >= 0xD800 and c <= 0xDFFF ) or c > 0x10FFFF )
        return { 0, 0 };
    return { c, length };
}
}

bool Utf8Valid( string_view text )
{
    for ( size_t pos = 0; pos < text.size(); )
    {
        const auto [c, length] = Decode( text, pos );
        if ( length == 0 )
            return false;
        pos += length;
    }
    return true;
}

char32_t Utf8Next( string_view text, size_t& pos )
{
    const auto [c, length] = Decode( text, pos );
    if ( length == 0 )
    {
        pos++;
        return cReplacementCharacter;
    }
    pos += length;
    return c;
}

void Utf8Append( string& out, char32_t c )
{
    if ( ( c >= 0xD800 and c <= 0xDFFF ) or c > 0x10FFFF )
        c = cReplacementCharacter;

    if ( c < 0x80 )
        out += (char)c;
    else if ( c < 0x800 )
    {
        out += (char)( 0xC0 | ( c >> 6 ) );
        out += (char)( 0x80 | ( c & 0x3F ) );
    }
    else if ( c < 0x10000 )
    {
        out += (char)( 0xE0 | ( c >> 12 ) );
        out += (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) );
        out += (char)( 0x80 | ( c & 0x3F ) );
    }
    else
    {
        out += (char)( 0xF0 | ( c >> 18 ) );
        out += (char)( 0x80 | ( ( c >> 12 ) & 0x3F ) );
        out += (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) );
        out += (char)( 0x80 | ( c & 0x3F ) );
    }
}

size_t Utf8CodePoints( string_view text )
{
    size_t count = 0;
    for ( size_t pos = 0; pos < text.size(); count++ )
        Utf8Next( text, pos );
    return count;
}

string_view Utf8Truncate( string_view text, size_t maxBytes )
{
    if ( text.size() <= maxBytes )
        return text;
    size_t end = maxBytes;
    // Back off the continuation bytes of the code point the cut falls in.
    while ( end > 0 and Continuation( (unsigned char)text[end] ) )
        end--;
    return text.substr( 0, end );
}

std::filesystem::path PathFromUtf8( string_view utf8 )
{
    return std::filesystem::path( std::u8string_view( reinterpret_cast<const char8_t*>( utf8.data() ), utf8.size() ) );
}

string PathToUtf8( const std::filesystem::path& path )
{
    const std::u8string u8 = path.u8string();
    return string( reinterpret_cast<const char*>( u8.data() ), u8.size() );
}
}
