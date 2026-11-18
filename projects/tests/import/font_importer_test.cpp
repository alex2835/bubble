// Fonts imported as an MSDF atlas - FreeType reading, msdfgen drawing - from
// a TrueType font made in code.
#include "bubble/import/font_importer.hpp"
#ifdef BUBBLE_IMPORT_FONTS
#include "bubble/assets/font_asset.hpp"
#include "bubble/types/algorithm.hpp"
#include "ui/test_font.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
expected<FontAsset, string> TryImport( string_view source, string_view settings = "{}" )
{
    auto importedMaybe =
        FontImporter( ".ttf" ).mImport( source, *Json::TryParse( settings ), *AssetPath::TryFrom( "fonts/test.ttf" ) );
    if ( not importedMaybe )
        return unexpected( std::move( importedMaybe.error() ) );
    return TryReadFontAsset( importedMaybe->mBytes );
}

// The median of an atlas texel's three channels, 0 to 1: above a half is
// inside a glyph.
f32 Median( const FontAsset& font, vec2 texel )
{
    const TextureMip& mip = font.mAtlas.mMips[0];
    const size_t at = ( static_cast<size_t>( texel.y ) * mip.mWidth + static_cast<size_t>( texel.x ) ) * 4;
    const array<f32, 3> channels{ mip.mPixels[at] / 255.0f, mip.mPixels[at + 1] / 255.0f,
                                  mip.mPixels[at + 2] / 255.0f };
    return max( min( channels[0], channels[1] ), min( max( channels[0], channels[1] ), channels[2] ) );
}
}

TEST_CASE( "A TrueType font is imported as an atlas of its glyphs with its metrics and kerning" )
{
    const auto fontMaybe = TryImport( test::TestFont() );
    REQUIRE_MESSAGE( fontMaybe, fontMaybe.error() );
    const FontAsset& font = *fontMaybe;
    CHECK( font.mSize == 48 );
    CHECK( font.mRange == 4 );
    CHECK( font.mAscender == doctest::Approx( 0.8f ) );
    CHECK( font.mDescender == doctest::Approx( -0.2f ) );
    CHECK( font.mLineHeight == doctest::Approx( 1.0f ) );
    CHECK_FALSE( font.mAtlas.mSrgb );

    // The characters the font has, of those asked for: nothing for the rest.
    vector<u32> characters;
    for ( const FontGlyph& glyph : font.mGlyphs )
        characters.push_back( glyph.mCodepoint );
    CHECK( characters == vector<u32>{ 0x20, 0x41, 0x56, 0x416 } );
    const FontGlyph& space = *font.TryGlyph( ' ' );
    CHECK( space.mAdvance == doctest::Approx( 0.3f ) );
    CHECK( space.mAtlas == vec4( 0.0f ) );

    // A's square, 0.1 to 0.6 across and 0 to 0.7 up, with room for the range
    // about it.
    const FontGlyph& a = *font.TryGlyph( 'A' );
    CHECK( a.mAdvance == doctest::Approx( 0.7f ) );
    CHECK( a.mPlane.x < 0.1f );
    CHECK( a.mPlane.z > 0.6f );
    CHECK( a.mPlane.w > 0.7f );
    // Inside in its middle, outside at its cell's corner.
    const vec2 middle = vec2( a.mAtlas.x + a.mAtlas.z, a.mAtlas.y + a.mAtlas.w ) * 0.5f;
    CHECK( Median( font, middle ) > 0.6f );
    CHECK( Median( font, vec2( a.mAtlas.x, a.mAtlas.y ) ) < 0.4f );
    // The square's edge where the plane says, its top at 0.7 em: the median
    // crosses a half there.
    const f32 topRow = a.mAtlas.y + ( a.mPlane.w - 0.7f ) * font.mSize;
    CHECK( Median( font, vec2( middle.x, topRow - 1.5f ) ) < 0.5f );
    CHECK( Median( font, vec2( middle.x, topRow + 1.5f ) ) > 0.5f );

    CHECK( font.Kerning( 'A', 'V' ) == doctest::Approx( -0.1f ) );
    CHECK( font.Kerning( 'V', 'A' ) == 0.0f );

    // Smaller, its cells smaller.
    const auto smallMaybe = TryImport( test::TestFont(), R"({ "size": 16, "range": 2 })" );
    REQUIRE_MESSAGE( smallMaybe, smallMaybe.error() );
    const FontGlyph& smallA = *smallMaybe->TryGlyph( 'A' );
    CHECK( smallA.mAtlas.z - smallA.mAtlas.x < a.mAtlas.z - a.mAtlas.x );
}

TEST_CASE( "A font that will not import, and settings that are wrong, say why" )
{
    CHECK( TryImport( "not a font" ).error() == "fonts/test.ttf: not a font FreeType reads" );
    CHECK( TryImport( test::TestFont(), R"({ "size": 2 })" ).error() ==
           "fonts/test.ttf: settings.size: 2 is not from 8 to 256 pixels" );
    CHECK( TryImport( test::TestFont(), R"({ "range": 0 })" ).error() ==
           "fonts/test.ttf: settings.range: 0 is not from 1 to 32 pixels" );
    CHECK( TryImport( test::TestFont(), R"({ "bold": true })" )
               .error()
               .ends_with( "settings.bold: not a field (fields: size, range, chars)" ) );
}
#endif
