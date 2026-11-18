#include "text_layout.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"

namespace bubble
{
namespace
{
struct Placed
{
    u32 mCodepoint = 0;
    f32 mX = 0;
};

struct Line
{
    vector<Placed> mChars;
    f32 mWidth = 0;
};
}

TextLayout LayOutText( string_view text, const FontAsset& font, f32 size, f32 wrapWidth, f32 boxWidth, TextAlign align )
{
    vector<u32> codepoints;
    for ( size_t at = 0; at < text.size(); )
        codepoints.push_back( static_cast<u32>( Utf8Next( text, at ) ) );

    // Greedy: a line takes characters while they fit, and goes back to its
    // last space when one does not.
    vector<Line> lines( 1 );
    f32 pen = 0;
    u32 previous = 0;
    const auto advanceOf = [&]( u32 c ) {
        const auto glyphMaybe = font.TryGlyph( c );
        return glyphMaybe ? glyphMaybe->mAdvance * size : 0.0f;
    };
    // The width of a line, its trailing spaces not counted.
    const auto widthOf = [&]( const Line& line ) {
        for ( size_t at = line.mChars.size(); at-- > 0; )
            if ( line.mChars[at].mCodepoint != ' ' )
                return line.mChars[at].mX + advanceOf( line.mChars[at].mCodepoint );
        return 0.0f;
    };
    const auto relayFrom = [&]( Line& from, size_t first ) {
        Line next;
        u32 before = 0;
        f32 x = 0;
        for ( size_t at = first; at < from.mChars.size(); ++at )
        {
            const u32 c = from.mChars[at].mCodepoint;
            x += before != 0 ? font.Kerning( before, c ) * size : 0.0f;
            next.mChars.push_back( Placed{ c, x } );
            x += advanceOf( c );
            before = c;
        }
        from.mChars.resize( first );
        pen = x;
        previous = before;
        return next;
    };
    for ( const u32 c : codepoints )
    {
        if ( c == '\n' )
        {
            lines.emplace_back();
            pen = 0;
            previous = 0;
            continue;
        }
        Line& line = lines.back();
        const f32 x = pen + ( previous != 0 ? font.Kerning( previous, c ) * size : 0.0f );
        const f32 end = x + advanceOf( c );
        if ( c != ' ' and end > wrapWidth and not line.mChars.empty() )
        {
            // Back to the last space, or, a word too long for a line, here.
            size_t space = line.mChars.size();
            while ( space > 0 and line.mChars[space - 1].mCodepoint != ' ' )
                --space;
            Line next = space > 0 ? relayFrom( line, space ) : Line{};
            if ( space == 0 )
            {
                pen = 0;
                previous = 0;
            }
            lines.push_back( std::move( next ) );
            Line& fresh = lines.back();
            const f32 freshX = pen + ( previous != 0 ? font.Kerning( previous, c ) * size : 0.0f );
            fresh.mChars.push_back( Placed{ c, freshX } );
            pen = freshX + advanceOf( c );
            previous = c;
            continue;
        }
        line.mChars.push_back( Placed{ c, x } );
        pen = end;
        previous = c;
    }

    TextLayout layout;
    const f32 lineHeight = font.mLineHeight * size;
    const vec2 atlasSize( static_cast<f32>( font.mAtlas.mMips[0].mWidth ),
                          static_cast<f32>( font.mAtlas.mMips[0].mHeight ) );
    for ( Line& line : lines )
        line.mWidth = widthOf( line );
    for ( const Line& line : lines )
        layout.mSize.x = max( layout.mSize.x, line.mWidth );
    layout.mSize.y = lineHeight * static_cast<f32>( lines.size() );
    const f32 across = boxWidth > 0 ? boxWidth : layout.mSize.x;
    for ( size_t l = 0; l < lines.size(); ++l )
    {
        const Line& line = lines[l];
        const f32 offset = align == TextAlign::Start ? 0.0f
                           : align == TextAlign::End ? across - line.mWidth
                                                     : ( across - line.mWidth ) * 0.5f;
        const f32 baseline = font.mAscender * size + lineHeight * static_cast<f32>( l );
        for ( const Placed& placed : line.mChars )
        {
            const auto glyphMaybe = font.TryGlyph( placed.mCodepoint );
            if ( not glyphMaybe or glyphMaybe->mAtlas.z <= glyphMaybe->mAtlas.x )
                continue;
            const vec4& plane = glyphMaybe->mPlane;
            const vec4& atlas = glyphMaybe->mAtlas;
            layout.mGlyphs.push_back(
                PlacedGlyph{ .mMin = vec2( offset + placed.mX + plane.x * size, baseline - plane.w * size ),
                             .mMax = vec2( offset + placed.mX + plane.z * size, baseline - plane.y * size ),
                             .mUvMin = vec2( atlas.x, atlas.y ) / atlasSize,
                             .mUvMax = vec2( atlas.z, atlas.w ) / atlasSize } );
        }
    }
    return layout;
}
}
