// Text set in a font: the ui module's own.
#pragma once
#include "bubble/assets/font_asset.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"

namespace bubble
{
// One glyph placed: its quad from the text's top left, Y down, and its part
// of the font's atlas.
struct PlacedGlyph
{
    vec2 mMin{ 0.0f };
    vec2 mMax{ 0.0f };
    vec2 mUvMin{ 0.0f };
    vec2 mUvMax{ 0.0f };
};

struct TextLayout
{
    vector<PlacedGlyph> mGlyphs;
    // The widest line, and the lines' height.
    vec2 mSize{ 0.0f };
};

enum class TextAlign : u8
{
    Start,
    Center,
    End,
};

// `text` in `font`, an em `size` pixels: lines broken at "\n", and wrapped at
// spaces to be no wider than `wrapWidth` - a word wider than that broken where
// it must -; each line put to `align` across `boxWidth`. Kerning between
// glyphs; a character the font lacks takes no room.
TextLayout LayOutText( string_view text,
                       const FontAsset& font,
                       f32 size,
                       f32 wrapWidth,
                       f32 boxWidth,
                       TextAlign align );
}
