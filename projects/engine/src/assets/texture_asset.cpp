#include "bubble/assets/texture_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BTEX";
constexpr u32 cVersion = 1;
constexpr u32 cSrgbFlag = 1;
}

string WriteTextureAsset( const TextureAsset& texture )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cVersion );
    out.U32( texture.mSrgb ? cSrgbFlag : 0 );
    out.U32( static_cast<u32>( texture.mMips.size() ) );
    for ( const TextureMip& mip : texture.mMips )
    {
        out.U32( mip.mWidth );
        out.U32( mip.mHeight );
        out.Bytes( mip.mPixels );
    }
    return out.Take();
}

expected<TextureAsset, string> TryReadTextureAsset( string_view bytes )
{
    BinaryReader reader( bytes );
    const auto cutShort = [] { return unexpected( "a texture cut short"s ); };
    const auto magicMaybe = reader.TryBytes( cMagic.size() );
    if ( not magicMaybe or *magicMaybe != cMagic )
        return unexpected( "not a texture the importer made"s );
    const auto versionMaybe = reader.TryU32();
    const auto flagsMaybe = reader.TryU32();
    const auto levelsMaybe = reader.TryU32();
    if ( not versionMaybe or not flagsMaybe or not levelsMaybe )
        return cutShort();
    if ( *versionMaybe != cVersion )
        return unexpected( format( "a texture of version {}, not {}: import it again", *versionMaybe, cVersion ) );

    TextureAsset texture;
    texture.mSrgb = ( *flagsMaybe & cSrgbFlag ) != 0;
    for ( u32 level = 0; level < *levelsMaybe; ++level )
    {
        const auto widthMaybe = reader.TryU32();
        const auto heightMaybe = reader.TryU32();
        if ( not widthMaybe or not heightMaybe )
            return cutShort();
        if ( *widthMaybe == 0 or *heightMaybe == 0 )
            return unexpected( format( "level {} of a texture is {}x{}", level, *widthMaybe, *heightMaybe ) );
        const auto pixelsMaybe = reader.TryBytes( u64{ *widthMaybe } * *heightMaybe * 4 );
        if ( not pixelsMaybe )
            return cutShort();
        texture.mMips.push_back( TextureMip{ .mWidth = *widthMaybe,
                                             .mHeight = *heightMaybe,
                                             .mPixels = vector<u8>( pixelsMaybe->begin(), pixelsMaybe->end() ) } );
    }
    if ( texture.mMips.empty() )
        return unexpected( "a texture with no levels"s );
    if ( not reader.AtEnd() )
        return unexpected( "a texture with more after its last level"s );
    return texture;
}

void RegisterTextureLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<TextureAsset>(
        cTextureKind, []( string_view bytes, const AssetPath& ) { return TryReadTextureAsset( bytes ); } );
}
}
