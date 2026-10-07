#pragma once
#include "bubble/types/filesystem.hpp"
#include "bubble/types/string.hpp"

// Every string in the engine is UTF-8 in a plain string. A string from
// outside - a file, typed text, a file name, Luau - is checked once, where it
// comes in; inside, it is trusted. size() counts bytes, which is right for
// memory, files, hashes and comparison. What a person counts as characters
// (graphemes) is the text module's business.
namespace bubble
{
inline constexpr char32_t cReplacementCharacter = U'�';

// Well-formed UTF-8: no stray continuation bytes, no overlong forms, no
// surrogates, nothing past U+10FFFF.
bool Utf8Valid( string_view text );

// The code point at `pos`, moving `pos` past it. An ill-formed sequence
// yields U+FFFD and moves one byte, so a loop always ends.
char32_t Utf8Next( string_view text, size_t& pos );

// Appends `c` encoded; a surrogate or a value past U+10FFFF appends U+FFFD.
void Utf8Append( string& out, char32_t c );

// How many code points - not characters as a person counts them: "e" with a
// combining accent is two, a thumbs-up with a skin tone is two.
size_t Utf8CodePoints( string_view text );

// The longest prefix of at most `maxBytes` that does not end inside a code
// point.
string_view Utf8Truncate( string_view text, size_t maxBytes );

// File system paths from and to UTF-8. Never build a path from a string
// directly: on Windows that reads the bytes in the ANSI code page.
OsPath PathFromUtf8( string_view utf8 );
string PathToUtf8( const OsPath& path );
}
