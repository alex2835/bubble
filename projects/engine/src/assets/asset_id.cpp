#include "bubble/assets/asset_id.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/random.hpp"

namespace bubble
{
namespace
{
opt<u64> TryParseHex( string_view text )
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
            return nullopt;
        value = value << 4 | digit;
    }
    return value;
}
}

AssetId AssetId::Generate()
{
    random_device entropy;
    const auto half = [&] { return u64{ entropy() } << 32 | u64{ entropy() }; };
    const u64 high = half();
    return AssetId{ high, half() };
}

opt<AssetId> AssetId::TryParse( string_view text )
{
    if ( text.size() != 32 )
        return nullopt;
    const auto highMaybe = TryParseHex( text.substr( 0, 16 ) );
    const auto lowMaybe = TryParseHex( text.substr( 16 ) );
    if ( not highMaybe or not lowMaybe )
        return nullopt;
    return AssetId{ *highMaybe, *lowMaybe };
}

string AssetId::ToString() const
{
    return format( "{:016x}{:016x}", mHigh, mLow );
}
}
