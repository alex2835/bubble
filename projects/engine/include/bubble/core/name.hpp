#pragma once
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
namespace detail
{
// FNV-1a, 32 bits: the hash EnTT names its types and members by, so a Name
// and an entt::id_type for the same text are the same number.
constexpr u32 NameHash( string_view text )
{
    u32 hash = 2166136261u;
    for ( const char c : text )
    {
        hash ^= static_cast<u8>( c );
        hash *= 16777619u;
    }
    return hash;
}

// ASCII snake_case: a lowercase letter, then lowercase letters, digits and _.
constexpr bool NameValid( string_view text )
{
    if ( text.empty() or text[0] < 'a' or text[0] > 'z' )
        return false;
    for ( const char c : text )
        if ( not( ( c >= 'a' and c <= 'z' ) or ( c >= '0' and c <= '9' ) or c == '_' ) )
            return false;
    return true;
}
}

// What the engine and its users name things by: components, fields,
// actions, parameters. Kept as a 32-bit hash, so comparing and hashing a
// Name is one number - as FName in Unreal and StringName in Godot.
//
// The text of every Name made by From lives in one table for the whole
// process - the third global, beside the log and the profiler - for logs,
// and to catch two texts with one hash. "jump"_n is checked and hashed at
// compile time and does not touch the table: its text is known once the same
// name is declared, which every name a file or a script can use is.
class Name
{
public:
    constexpr Name() = default;

    // A name from outside - a file, a script, a declaration: checked, and
    // its text put in the table.
    static expected<Name, string> From( string_view text );

    constexpr u32 Hash() const { return mHash; }
    constexpr bool Empty() const { return mHash == 0; }
    // The text; "#" and the hash for one nobody declared.
    string ToString() const;

    friend constexpr bool operator==( Name, Name ) = default;
    friend constexpr auto operator<=>( Name, Name ) = default;

private:
    friend consteval Name operator""_n( const char* text, size_t size );
    constexpr explicit Name( u32 hash ) : mHash( hash ) {}

    u32 mHash = 0;
};

consteval Name operator""_n( const char* text, size_t size )
{
    const string_view name( text, size );
    if ( not detail::NameValid( name ) )
        throw "a name is ASCII snake_case: a lowercase letter, then lowercase letters, digits and _";
    if ( detail::NameHash( name ) == 0 )
        throw "this name hashes to 0, the empty name's hash";
    return Name( detail::NameHash( name ) );
}
}

template <>
struct std::hash<bubble::Name>
{
    size_t operator()( bubble::Name name ) const { return name.Hash(); }
};
