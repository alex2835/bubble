#include "bubble/import/font_importer.hpp"
#ifdef BUBBLE_IMPORT_FONTS
#include "bubble/assets/font_asset.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/value_json.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <ext/import-font.h>
#include <msdfgen.h>

namespace bubble
{
namespace
{
struct FontSettings
{
    f32 mSize = 48;
    f32 mRange = 4;
    string mChars;
};

expected<FontSettings, string> TryReadSettings( const Json& json )
{
    const ValueType number = ValueType::Of( ValueKind::Number );
    const ValueType type = ValueType::RecordOf( { { Name::Declare( "size" ), number },
                                                  { Name::Declare( "range" ), number },
                                                  { Name::Declare( "chars" ), ValueType::Of( ValueKind::Text ) } } );
    const AssetCatalog none;
    auto valueMaybe = TryValueFromJson( json, type, "settings", none );
    if ( not valueMaybe )
        return unexpected( std::move( valueMaybe.error() ) );
    FontSettings settings;
    for ( const ValueField& field : *valueMaybe->TryAs<Value::Record>() )
    {
        if ( field.mName == "chars"_n )
        {
            settings.mChars = field.mValue.TryAs<Text>()->String();
            continue;
        }
        const f32 value = static_cast<f32>( *field.mValue.TryAs<f64>() );
        if ( field.mName == "size"_n )
        {
            if ( not( value >= 8 and value <= 256 ) )
                return unexpected( format( "settings.size: {} is not from 8 to 256 pixels", value ) );
            settings.mSize = value;
        }
        else
        {
            if ( not( value >= 1 and value <= 32 ) )
                return unexpected( format( "settings.range: {} is not from 1 to 32 pixels", value ) );
            settings.mRange = value;
        }
    }
    return settings;
}

// What a game's text is mostly made of: Latin with Latin-1, Cyrillic, the
// common punctuation; and the settings' own.
vector<u32> Charset( string_view extra )
{
    vector<u32> chars;
    for ( u32 c = 0x20; c <= 0x7E; ++c )
        chars.push_back( c );
    for ( u32 c = 0xA0; c <= 0xFF; ++c )
        chars.push_back( c );
    for ( u32 c = 0x400; c <= 0x45F; ++c )
        chars.push_back( c );
    for ( const u32 c : { 0x2013u, 0x2014u, 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x2026u, 0x2116u, 0x20ACu } )
        chars.push_back( c );
    for ( size_t at = 0; at < extra.size(); )
        chars.push_back( static_cast<u32>( Utf8Next( extra, at ) ) );
    sort( chars.begin(), chars.end() );
    chars.erase( unique( chars.begin(), chars.end() ), chars.end() );
    return chars;
}

// FreeType and the font, let go of on every way out.
struct Font
{
    msdfgen::FreetypeHandle* mLibrary = nullptr;
    msdfgen::FontHandle* mFont = nullptr;

    Font() = default;
    Font( const Font& ) = delete;
    Font& operator=( const Font& ) = delete;
    ~Font()
    {
        if ( mFont != nullptr )
            msdfgen::destroyFont( mFont );
        if ( mLibrary != nullptr )
            msdfgen::deinitializeFreetype( mLibrary );
    }
};

// A glyph drawn, before it has a place in the atlas.
struct Drawn
{
    FontGlyph mGlyph;
    i32 mWidth = 0;
    i32 mHeight = 0;
    // RGB, rows from the top.
    vector<u8> mPixels;
};

u8 ToByte( f32 value )
{
    return static_cast<u8>( clamp( value * 255.0f + 0.5f, 0.0f, 255.0f ) );
}

expected<string, string> TryImportFont( string_view source, const Json& settingsJson, const AssetPath& path )
{
    BUBBLE_PROFILE_ZONE();
    const auto settingsMaybe = TryReadSettings( settingsJson );
    if ( not settingsMaybe )
        return unexpected( format( "{}: {}", path.View(), settingsMaybe.error() ) );
    const FontSettings& settings = *settingsMaybe;

    Font font;
    font.mLibrary = msdfgen::initializeFreetype();
    if ( font.mLibrary == nullptr )
        return unexpected( format( "{}: FreeType would not start", path.View() ) );
    font.mFont = msdfgen::loadFontData( font.mLibrary, reinterpret_cast<const msdfgen::byte*>( source.data() ),
                                        static_cast<int>( source.size() ) );
    if ( font.mFont == nullptr )
        return unexpected( format( "{}: not a font FreeType reads", path.View() ) );
    msdfgen::FontMetrics metrics{};
    if ( not msdfgen::getFontMetrics( metrics, font.mFont, msdfgen::FONT_SCALING_EM_NORMALIZED ) )
        return unexpected( format( "{}: it has no metrics", path.View() ) );

    FontAsset asset;
    asset.mSize = settings.mSize;
    asset.mRange = settings.mRange;
    asset.mAscender = static_cast<f32>( metrics.ascenderY );
    asset.mDescender = static_cast<f32>( metrics.descenderY );
    asset.mLineHeight = static_cast<f32>( metrics.lineHeight );

    // Each glyph in a cell of its own: its box, and half the range and a
    // pixel about it, where the distance fades out.
    const f64 scale = settings.mSize;
    const f64 pad = settings.mRange / 2.0 + 1.0;
    vector<Drawn> drawn;
    vector<u32> present;
    for ( const u32 c : Charset( settings.mChars ) )
    {
        msdfgen::GlyphIndex index;
        if ( not msdfgen::getGlyphIndex( index, font.mFont, c ) or index.getIndex() == 0 )
            continue;
        msdfgen::Shape shape;
        f64 advance = 0;
        if ( not msdfgen::loadGlyph( shape, font.mFont, index, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance ) )
            continue;
        present.push_back( c );
        Drawn glyph;
        glyph.mGlyph.mCodepoint = c;
        glyph.mGlyph.mAdvance = static_cast<f32>( advance );
        shape.normalize();
        const msdfgen::Shape::Bounds bounds = shape.getBounds();
        if ( shape.contours.empty() or not shape.validate() or bounds.r <= bounds.l or bounds.t <= bounds.b )
        {
            drawn.push_back( std::move( glyph ) );
            continue;
        }
        msdfgen::edgeColoringSimple( shape, 3.0 );
        glyph.mWidth = static_cast<i32>( ceil( ( bounds.r - bounds.l ) * scale + 2.0 * pad ) );
        glyph.mHeight = static_cast<i32>( ceil( ( bounds.t - bounds.b ) * scale + 2.0 * pad ) );
        const msdfgen::Vector2 translate( pad / scale - bounds.l, pad / scale - bounds.b );
        msdfgen::Bitmap<float, 3> bitmap( glyph.mWidth, glyph.mHeight );
        msdfgen::generateMSDF(
            bitmap, shape,
            msdfgen::SDFTransformation( msdfgen::Projection( scale, translate ),
                                        msdfgen::DistanceMapping( msdfgen::Range( settings.mRange / scale ) ) ) );
        // msdfgen's rows go up from the bottom; an atlas's down from the top.
        glyph.mPixels.resize( static_cast<size_t>( glyph.mWidth * glyph.mHeight * 3 ) );
        for ( i32 y = 0; y < glyph.mHeight; ++y )
            for ( i32 x = 0; x < glyph.mWidth; ++x )
            {
                const float* texel = bitmap( x, glyph.mHeight - 1 - y );
                for ( i32 channel = 0; channel < 3; ++channel )
                    glyph.mPixels[static_cast<size_t>( ( y * glyph.mWidth + x ) * 3 + channel )] =
                        ToByte( texel[channel] );
            }
        const f32 left = static_cast<f32>( bounds.l - pad / scale );
        const f32 bottom = static_cast<f32>( bounds.b - pad / scale );
        glyph.mGlyph.mPlane = vec4( left, bottom, left + static_cast<f32>( glyph.mWidth / scale ),
                                    bottom + static_cast<f32>( glyph.mHeight / scale ) );
        drawn.push_back( std::move( glyph ) );
    }
    if ( present.empty() )
        return unexpected( format( "{}: none of the characters asked for is in it", path.View() ) );

    // Shelves of glyphs, the tallest first, in an atlas as wide as the
    // glyphs' area asks.
    u64 area = 0;
    for ( const Drawn& glyph : drawn )
        area += static_cast<u64>( ( glyph.mWidth + 1 ) * ( glyph.mHeight + 1 ) );
    u32 width = 256;
    while ( width < 4096 and u64{ width } * width < area * 2 )
        width *= 2;
    vector<size_t> order( drawn.size() );
    for ( size_t at = 0; at < order.size(); ++at )
        order[at] = at;
    stable_sort( order.begin(), order.end(),
                 [&]( size_t a, size_t b ) { return drawn[a].mHeight > drawn[b].mHeight; } );
    i32 x = 0;
    i32 y = 0;
    i32 shelf = 0;
    for ( const size_t at : order )
    {
        Drawn& glyph = drawn[at];
        if ( glyph.mWidth == 0 )
            continue;
        if ( glyph.mWidth > static_cast<i32>( width ) )
            return unexpected( format( "{}: U+{:04X} is wider than an atlas can be: a smaller size", path.View(),
                                       glyph.mGlyph.mCodepoint ) );
        if ( x + glyph.mWidth > static_cast<i32>( width ) )
        {
            x = 0;
            y += shelf + 1;
            shelf = 0;
        }
        glyph.mGlyph.mAtlas = vec4( static_cast<f32>( x ), static_cast<f32>( y ), static_cast<f32>( x + glyph.mWidth ),
                                    static_cast<f32>( y + glyph.mHeight ) );
        x += glyph.mWidth + 1;
        shelf = max( shelf, glyph.mHeight );
    }
    const auto height = static_cast<u32>( ( y + shelf + 3 ) / 4 * 4 );
    TextureMip mip{ .mWidth = width, .mHeight = max( height, 4u ), .mPixels = {} };
    mip.mPixels.assign( size_t{ mip.mWidth } * mip.mHeight * 4, 0 );
    for ( size_t texel = 3; texel < mip.mPixels.size(); texel += 4 )
        mip.mPixels[texel] = 255;
    for ( const Drawn& glyph : drawn )
    {
        if ( glyph.mWidth == 0 )
            continue;
        const auto left = static_cast<size_t>( glyph.mGlyph.mAtlas.x );
        const auto top = static_cast<size_t>( glyph.mGlyph.mAtlas.y );
        for ( size_t row = 0; row < static_cast<size_t>( glyph.mHeight ); ++row )
            for ( size_t column = 0; column < static_cast<size_t>( glyph.mWidth ); ++column )
                for ( size_t channel = 0; channel < 3; ++channel )
                    mip.mPixels[( ( top + row ) * mip.mWidth + left + column ) * 4 + channel] =
                        glyph.mPixels[( row * static_cast<size_t>( glyph.mWidth ) + column ) * 3 + channel];
    }
    asset.mAtlas = TextureAsset{ .mSrgb = false, .mMips = { std::move( mip ) } };
    for ( Drawn& glyph : drawn )
        asset.mGlyphs.push_back( glyph.mGlyph );

    for ( const u32 first : present )
        for ( const u32 second : present )
        {
            f64 kerning = 0;
            if ( msdfgen::getKerning( kerning, font.mFont, first, second, msdfgen::FONT_SCALING_EM_NORMALIZED ) and
                 kerning != 0 )
                asset.mKerning.push_back(
                    FontKerning{ .mFirst = first, .mSecond = second, .mAdvance = static_cast<f32>( kerning ) } );
        }
    return WriteFontAsset( asset );
}
}

Importer FontImporter( string_view extension )
{
    return Importer{ .mExtension = string( extension ),
                     .mKind = string( cFontKind ),
                     .mVersion = 1,
                     .mImport = []( string_view source, const Json& settings,
                                    const AssetPath& path ) -> expected<ImportedAsset, string> {
                         auto fontMaybe = TryImportFont( source, settings, path );
                         if ( not fontMaybe )
                             return unexpected( std::move( fontMaybe.error() ) );
                         return ImportedAsset( std::move( *fontMaybe ) );
                     },
                     .mSettings = {
                         ImportSetting{ .mName = "size",
                                        .mType = ValueType::Of( ValueKind::Number ),
                                        .mDefault = Value( 48.0 ),
                                        .mTooltip = "A glyph's height in the atlas, from 8 to 256 pixels: sharper "
                                                    "corners when larger" },
                         ImportSetting{ .mName = "range",
                                        .mType = ValueType::Of( ValueKind::Number ),
                                        .mDefault = Value( 4.0 ),
                                        .mTooltip = "How far from an edge the atlas knows it, from 1 to 32 pixels: "
                                                    "room for outlines and glow" },
                         ImportSetting{ .mName = "chars",
                                        .mType = ValueType::Of( ValueKind::Text ),
                                        .mDefault = Value( *Text::TryFrom( "" ) ),
                                        .mTooltip = "Characters beyond Latin, Latin-1 and Cyrillic, as text" } } };
}
}
#endif
