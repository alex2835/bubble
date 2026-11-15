#include "bubble/assets/texture_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BTEX";
constexpr u32 cVersion = 1;
constexpr u32 cSrgbFlag = 1;

void Append( string& out, u32 value )
{
    for ( u32 shift = 0; shift < 32; shift += 8 )
        out.push_back( static_cast<char>( ( value >> shift ) & 0xff ) );
}

// Reads from the front of the bytes, refusing to go past their end.
class Reader
{
public:
    explicit Reader( string_view bytes ) : mBytes( bytes ) {}

    opt<u32> TryU32()
    {
        if ( mBytes.size() - mAt < 4 )
            return nullopt;
        u32 value = 0;
        for ( u32 i = 0; i < 4; ++i )
            value |= u32{ static_cast<u8>( mBytes[mAt + i] ) } << ( 8 * i );
        mAt += 4;
        return value;
    }

    opt<string_view> TryBytes( size_t count )
    {
        if ( mBytes.size() - mAt < count )
            return nullopt;
        const string_view taken = mBytes.substr( mAt, count );
        mAt += count;
        return taken;
    }

    bool AtEnd() const { return mAt == mBytes.size(); }

private:
    string_view mBytes;
    size_t mAt = 0;
};
}

string WriteTextureAsset( const TextureAsset& texture )
{
    string out( cMagic );
    Append( out, cVersion );
    Append( out, texture.mSrgb ? cSrgbFlag : 0 );
    Append( out, static_cast<u32>( texture.mMips.size() ) );
    for ( const TextureMip& mip : texture.mMips )
    {
        Append( out, mip.mWidth );
        Append( out, mip.mHeight );
        out.append( mip.mPixels.begin(), mip.mPixels.end() );
    }
    return out;
}

expected<TextureAsset, string> TryReadTextureAsset( string_view bytes )
{
    Reader reader( bytes );
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
        // In 64 bits: a 32-bit size_t would wrap before it fails.
        const u64 size = u64{ *widthMaybe } * *heightMaybe * 4;
        if ( size > bytes.size() )
            return cutShort();
        const auto pixelsMaybe = reader.TryBytes( static_cast<size_t>( size ) );
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
