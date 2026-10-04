#include "bubble/assets/asset_id.hpp"
#include <format>

namespace bubble
{
namespace
{
// FNV-1a, 64 bits, from a given basis: two bases give two halves.
u64 Fnv( string_view text, u64 basis )
{
    u64 hash = basis;
    for ( const char c : text )
    {
        hash ^= static_cast<u8>( c );
        hash *= 0x100000001b3ull;
    }
    return hash;
}

opt<u64> ParseHex( string_view text )
{
    u64 value = 0;
    for ( const char c : text )
    {
        u64 digit = 0;
        if ( c >= '0' and c <= '9' )
            digit = static_cast<u64>( c - '0' );
        else if ( c >= 'a' and c <= 'f' )
            digit = static_cast<u64>( c - 'a' + 10 );
        else
            return std::nullopt;
        value = value << 4 | digit;
    }
    return value;
}
}

AssetId AssetId::MakeFrom( string_view path )
{
    return AssetId{ Fnv( path, 0xcbf29ce484222325ull ), Fnv( path, 0x84222325cbf29ce4ull ) };
}

opt<AssetId> AssetId::Parse( string_view text )
{
    if ( text.size() != 32 )
        return std::nullopt;
    const auto high = ParseHex( text.substr( 0, 16 ) );
    const auto low = ParseHex( text.substr( 16 ) );
    if ( not high or not low )
        return std::nullopt;
    return AssetId{ *high, *low };
}

string AssetId::ToString() const
{
    return std::format( "{:016x}{:016x}", mHigh, mLow );
}
}
