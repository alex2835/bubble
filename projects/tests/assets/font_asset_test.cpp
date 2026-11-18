// A font's bytes, as the importer writes them and the loader reads them.
#include "bubble/assets/font_asset.hpp"
#include "bubble/types/algorithm.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
FontAsset Font()
{
    FontAsset font{ .mAtlas =
                        TextureAsset{ .mSrgb = false,
                                      .mMips = { TextureMip{
                                          .mWidth = 4, .mHeight = 4, .mPixels = vector<u8>( 4 * 4 * 4, 200 ) } } },
                    .mSize = 32,
                    .mRange = 4,
                    .mAscender = 0.8f,
                    .mDescender = -0.2f,
                    .mLineHeight = 1.0f,
                    .mGlyphs = {},
                    .mKerning = {} };
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = ' ', .mAdvance = 0.25f } );
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = 'A',
                                       .mAdvance = 0.5f,
                                       .mPlane = vec4( 0.0f, 0.0f, 0.5f, 0.7f ),
                                       .mAtlas = vec4( 0.0f, 0.0f, 4.0f, 4.0f ) } );
    font.mGlyphs.push_back( FontGlyph{ .mCodepoint = 0x416, .mAdvance = 0.6f } );
    font.mKerning.push_back( FontKerning{ .mFirst = 'A', .mSecond = 'A', .mAdvance = -0.05f } );
    return font;
}
}

TEST_CASE( "A font's bytes read back as written, glyphs and kerning found by their characters" )
{
    const string bytes = WriteFontAsset( Font() );
    const auto readMaybe = TryReadFontAsset( bytes );
    REQUIRE_MESSAGE( readMaybe, readMaybe.error() );
    const FontAsset& font = *readMaybe;
    CHECK( font.mSize == 32 );
    CHECK( font.mDescender == -0.2f );
    REQUIRE( font.mGlyphs.size() == 3 );
    CHECK( font.TryGlyph( 'A' )->mPlane == vec4( 0.0f, 0.0f, 0.5f, 0.7f ) );
    CHECK( font.TryGlyph( 0x416 )->mAdvance == 0.6f );
    CHECK_FALSE( font.TryGlyph( 'B' ) );
    CHECK( font.Kerning( 'A', 'A' ) == -0.05f );
    CHECK( font.Kerning( 'A', ' ' ) == 0.0f );
    CHECK_FALSE( font.mAtlas.mSrgb );
    CHECK( font.mAtlas.mMips[0].mPixels.size() == 64 );

    CHECK( TryReadFontAsset( bytes.substr( 0, 30 ) ).error() == "a font cut short" );
    CHECK( TryReadFontAsset( bytes + "x" ).error() == "a font with more after its atlas" );
    CHECK( TryReadFontAsset( "TTF!" ).error() == "not a font the importer made" );
    FontAsset unordered = Font();
    ranges::reverse( unordered.mGlyphs );
    CHECK( TryReadFontAsset( WriteFontAsset( unordered ) ).error() == "a font's glyphs out of order at U+0041" );
}
