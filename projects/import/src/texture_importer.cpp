#include "bubble/import/texture_importer.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/math.hpp"
#include <stb_image.h>

namespace bubble
{
namespace
{
struct TextureSettings
{
    bool mSrgb = true;
    bool mMips = true;
};

expected<TextureSettings, string> TryReadSettings( const Json& json )
{
    const ValueType type = ValueType::RecordOf(
        { { "srgb"_n, ValueType::Of( ValueKind::Bool ) }, { "mips"_n, ValueType::Of( ValueKind::Bool ) } } );
    const AssetCatalog none;
    auto valueMaybe = TryValueFromJson( json, type, "settings", none );
    if ( not valueMaybe )
        return unexpected( std::move( valueMaybe.error() ) );
    TextureSettings settings;
    for ( const ValueField& field : *valueMaybe->TryAs<Value::Record>() )
    {
        const bool flag = *field.mValue.TryAs<bool>();
        if ( field.mName == "srgb"_n )
            settings.mSrgb = flag;
        else
            settings.mMips = flag;
    }
    return settings;
}

f32 ToLinear( u8 stored )
{
    const f32 v = static_cast<f32>( stored ) / 255.0f;
    return v <= 0.04045f ? v / 12.92f : pow( ( v + 0.055f ) / 1.055f, 2.4f );
}

u8 FromLinear( f32 linear )
{
    const f32 v = clamp( linear, 0.0f, 1.0f );
    const f32 stored = v <= 0.0031308f ? v * 12.92f : 1.055f * pow( v, 1.0f / 2.4f ) - 0.055f;
    return static_cast<u8>( round( stored * 255.0f ) );
}

// The level below `above`: each pixel the mean of the 2×2 over it - at an
// odd edge, the last row or column again. A color image's colors are
// averaged as light, alpha and data as stored.
TextureMip Halve( const TextureMip& above, bool srgb )
{
    TextureMip below{ .mWidth = max( above.mWidth / 2, 1u ), .mHeight = max( above.mHeight / 2, 1u ), .mPixels = {} };
    below.mPixels.resize( size_t{ below.mWidth } * below.mHeight * 4 );
    for ( u32 y = 0; y < below.mHeight; ++y )
        for ( u32 x = 0; x < below.mWidth; ++x )
            for ( u32 channel = 0; channel < 4; ++channel )
            {
                const bool light = srgb and channel < 3;
                f32 sum = 0;
                for ( const u32 sy : { 2 * y, min( 2 * y + 1, above.mHeight - 1 ) } )
                    for ( const u32 sx : { 2 * x, min( 2 * x + 1, above.mWidth - 1 ) } )
                    {
                        const u8 stored = above.mPixels[( size_t{ sy } * above.mWidth + sx ) * 4 + channel];
                        sum += light ? ToLinear( stored ) : static_cast<f32>( stored ) / 255.0f;
                    }
                const f32 mean = sum / 4.0f;
                below.mPixels[( size_t{ y } * below.mWidth + x ) * 4 + channel] =
                    light ? FromLinear( mean ) : static_cast<u8>( round( mean * 255.0f ) );
            }
    return below;
}

expected<string, string> TryImportTexture( string_view source, const Json& settingsJson, const AssetPath& path )
{
    BUBBLE_PROFILE_ZONE();
    const auto settingsMaybe = TryReadSettings( settingsJson );
    if ( not settingsMaybe )
        return unexpected( format( "{}: {}", path.View(), settingsMaybe.error() ) );
    if ( source.size() > static_cast<size_t>( numeric_limits<int>::max() ) )
        return unexpected( format( "{}: too big for the image decoder", path.View() ) );

    int width = 0;
    int height = 0;
    int channels = 0;
    // stb's C interface: bytes in, RGBA out, freed by it.
    stbi_uc* pixels = stbi_load_from_memory( reinterpret_cast<const stbi_uc*>( source.data() ),
                                             static_cast<int>( source.size() ), &width, &height, &channels, 4 );
    if ( pixels == nullptr )
        return unexpected( format( "{}: {}", path.View(), stbi_failure_reason() ) );
    TextureMip full{ .mWidth = static_cast<u32>( width ), .mHeight = static_cast<u32>( height ), .mPixels = {} };
    full.mPixels.assign( pixels, pixels + size_t{ full.mWidth } * full.mHeight * 4 );
    stbi_image_free( pixels );

    TextureAsset texture{ .mSrgb = settingsMaybe->mSrgb, .mMips = { std::move( full ) } };
    if ( settingsMaybe->mMips )
        while ( texture.mMips.back().mWidth > 1 or texture.mMips.back().mHeight > 1 )
            texture.mMips.push_back( Halve( texture.mMips.back(), texture.mSrgb ) );
    return WriteTextureAsset( texture );
}
}

vector<Importer> TextureImporters()
{
    vector<Importer> importers;
    for ( const string_view extension : { ".png", ".jpg", ".jpeg", ".tga", ".bmp" } )
        importers.push_back( Importer{ .mExtension = string( extension ),
                                       .mKind = string( cTextureKind ),
                                       .mVersion = 1,
                                       .mImport = &TryImportTexture } );
    return importers;
}
}
