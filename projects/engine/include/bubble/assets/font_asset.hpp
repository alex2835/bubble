#pragma once
#include "bubble/assets/texture_asset.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class AssetRegistry;

// One glyph of a font: how far it moves the pen, where it lies about the pen
// - left, bottom, right, top, in ems, Y up - and where it lies in the atlas,
// in its pixels: left, top, right, bottom. A glyph with nothing to draw - a
// space - has no box.
struct FontGlyph
{
    u32 mCodepoint = 0;
    f32 mAdvance = 0;
    vec4 mPlane{ 0.0f };
    vec4 mAtlas{ 0.0f };
};

// How much closer two glyphs stand than their advances say, in ems.
struct FontKerning
{
    u32 mFirst = 0;
    u32 mSecond = 0;
    f32 mAdvance = 0;
};

// A font as the importer made it: its glyphs as a multi-channel signed
// distance field in one atlas, which draws them sharp at any size. The
// distance spans mRange of the atlas's pixels each way; mSize is how many
// atlas pixels an em was drawn at. Lengths are in ems: a font drawn 20
// pixels high multiplies them by 20.
struct FontAsset
{
    // RGB as data, one level: what the MSDF shader reads.
    TextureAsset mAtlas;
    f32 mSize = 0;
    f32 mRange = 0;
    f32 mAscender = 0;
    f32 mDescender = 0;
    f32 mLineHeight = 0;
    // In order of their codepoints.
    vector<FontGlyph> mGlyphs;
    // In order of their pairs.
    vector<FontKerning> mKerning;

    OptRef<const FontGlyph> TryGlyph( u32 codepoint ) const;
    f32 Kerning( u32 first, u32 second ) const;
};

constexpr string_view cFontKind = "font";

// "BFNT", the version, the metrics, the glyphs, the kerning, then the atlas
// as a texture's bytes, little-endian.
string WriteFontAsset( const FontAsset& font );
// The error says what is wrong: cut short, another version, glyphs out of
// order, an atlas that is not one.
expected<FontAsset, string> TryReadFontAsset( string_view bytes );

// Teaches the registry fonts: <id>.font.
void RegisterFontLoader( AssetRegistry& registry );
}
