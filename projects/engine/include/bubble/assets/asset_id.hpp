#pragma once
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// An asset's identity: 128 bits, written to files as 32 hex digits. It is
// what components and scenes keep - the path can change, the id does not.
// A source file's is in the .import beside it; an engine file - a level, a
// prefab - keeps its own inside.
struct AssetId
{
    u64 mHigh = 0;
    u64 mLow = 0;

    // A new one, random: two files never share one by chance.
    static AssetId Generate();
    static opt<AssetId> TryParse( string_view text );
    string ToString() const;

    explicit operator bool() const { return mHigh != 0 or mLow != 0; }
    friend bool operator==( const AssetId&, const AssetId& ) = default;
    friend auto operator<=>( const AssetId&, const AssetId& ) = default;
};
}

template <>
struct std::hash<bubble::AssetId>
{
    size_t operator()( const bubble::AssetId& assetId ) const
    {
        return std::hash<bubble::u64>{}( assetId.mHigh ^ ( assetId.mLow * 0x9e3779b97f4a7c15ull ) );
    }
};
