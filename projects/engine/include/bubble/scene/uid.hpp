#pragma once
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// What an entity is called for good: in the level file, in a prefab, in
// saves and in links between objects. 64 random bits, written as 16 hex
// digits; never zero. The Entity is the scene's number for it this run.
struct Uid
{
    u64 mValue = 0;

    static opt<Uid> Parse( string_view text );
    string ToString() const;

    explicit operator bool() const { return mValue != 0; }
    friend bool operator==( Uid, Uid ) = default;
    friend auto operator<=>( Uid, Uid ) = default;
};
}

template <>
struct std::hash<bubble::Uid>
{
    size_t operator()( bubble::Uid uid ) const { return std::hash<bubble::u64>{}( uid.mValue ); }
};
