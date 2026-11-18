#pragma once
// A small TrueType font made in code - no font files in the repository: an
// em of 1000 units, ascender 800, descender -200; a space 300 wide; "A" a
// square, "V" a triangle and "Ж" a square, each 700 wide; "AV" kerned -100.
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

namespace bubble::test
{
namespace detail
{
// Big-endian, as TrueType has it.
struct FontWriter
{
    string mBytes;

    void U16( u32 value )
    {
        mBytes.push_back( static_cast<char>( ( value >> 8 ) & 0xFF ) );
        mBytes.push_back( static_cast<char>( value & 0xFF ) );
    }
    void I16( i32 value ) { U16( static_cast<u32>( static_cast<u16>( static_cast<i16>( value ) ) ) ); }
    void U32( u32 value )
    {
        U16( value >> 16 );
        U16( value & 0xFFFF );
    }
    void Pad( size_t to )
    {
        while ( mBytes.size() % to != 0 )
            mBytes.push_back( '\0' );
    }
};

struct TestGlyph
{
    u32 mCodepoint = 0;
    i32 mAdvance = 0;
    // One closed contour, its points on the curve; none for nothing to draw.
    vector<pair<i32, i32>> mPoints;
};
}

inline string TestFont()
{
    using detail::FontWriter;
    const vector<detail::TestGlyph> glyphs{
        detail::TestGlyph{ 0, 500, {} },
        detail::TestGlyph{ 0x20, 300, {} },
        detail::TestGlyph{ 0x41, 700, { { 100, 0 }, { 100, 700 }, { 600, 700 }, { 600, 0 } } },
        detail::TestGlyph{ 0x56, 700, { { 350, 0 }, { 100, 700 }, { 600, 700 } } },
        detail::TestGlyph{ 0x416, 700, { { 50, 0 }, { 50, 700 }, { 650, 700 }, { 650, 0 } } },
    };
    const auto count = static_cast<u32>( glyphs.size() );

    FontWriter glyf;
    vector<u32> offsets;
    for ( const detail::TestGlyph& glyph : glyphs )
    {
        offsets.push_back( static_cast<u32>( glyf.mBytes.size() ) );
        if ( glyph.mPoints.empty() )
            continue;
        i32 xMin = 1 << 20, yMin = 1 << 20, xMax = -( 1 << 20 ), yMax = -( 1 << 20 );
        for ( const auto& [x, y] : glyph.mPoints )
        {
            xMin = x < xMin ? x : xMin;
            yMin = y < yMin ? y : yMin;
            xMax = x > xMax ? x : xMax;
            yMax = y > yMax ? y : yMax;
        }
        glyf.I16( 1 );
        glyf.I16( xMin );
        glyf.I16( yMin );
        glyf.I16( xMax );
        glyf.I16( yMax );
        glyf.U16( static_cast<u32>( glyph.mPoints.size() - 1 ) );
        glyf.U16( 0 );
        // On the curve, each coordinate a 16-bit step from the last.
        for ( size_t point = 0; point < glyph.mPoints.size(); ++point )
            glyf.mBytes.push_back( '\x01' );
        i32 last = 0;
        for ( const auto& [x, y] : glyph.mPoints )
        {
            glyf.I16( x - last );
            last = x;
        }
        last = 0;
        for ( const auto& [x, y] : glyph.mPoints )
        {
            glyf.I16( y - last );
            last = y;
        }
        glyf.Pad( 2 );
    }
    offsets.push_back( static_cast<u32>( glyf.mBytes.size() ) );

    FontWriter loca;
    for ( const u32 offset : offsets )
        loca.U16( offset / 2 );

    FontWriter head;
    head.U32( 0x00010000 );
    head.U32( 0x00010000 );
    head.U32( 0 );
    head.U32( 0x5F0F3CF5 );
    head.U16( 0x000B );
    head.U16( 1000 );
    for ( u32 date = 0; date < 4; ++date )
        head.U32( 0 );
    head.I16( 50 );
    head.I16( 0 );
    head.I16( 650 );
    head.I16( 700 );
    head.U16( 0 );
    head.U16( 8 );
    head.I16( 2 );
    head.I16( 0 );
    head.I16( 0 );

    FontWriter hhea;
    hhea.U32( 0x00010000 );
    hhea.I16( 800 );
    hhea.I16( -200 );
    hhea.I16( 0 );
    hhea.U16( 700 );
    hhea.I16( 50 );
    hhea.I16( 50 );
    hhea.I16( 650 );
    hhea.I16( 1 );
    hhea.I16( 0 );
    hhea.I16( 0 );
    for ( u32 reserved = 0; reserved < 4; ++reserved )
        hhea.I16( 0 );
    hhea.I16( 0 );
    hhea.U16( count );

    FontWriter hmtx;
    for ( const detail::TestGlyph& glyph : glyphs )
    {
        hmtx.U16( static_cast<u32>( glyph.mAdvance ) );
        hmtx.I16( glyph.mPoints.empty() ? 0 : 50 );
    }

    FontWriter maxp;
    maxp.U32( 0x00010000 );
    maxp.U16( count );
    maxp.U16( 4 );
    maxp.U16( 1 );
    for ( u32 field = 0; field < 2; ++field )
        maxp.U16( 0 );
    maxp.U16( 2 );
    for ( u32 field = 0; field < 8; ++field )
        maxp.U16( 0 );

    // Format 4: a segment a character, then the end.
    FontWriter cmap;
    vector<pair<u32, u32>> mapped;
    for ( u32 glyph = 1; glyph < count; ++glyph )
        mapped.emplace_back( glyphs[glyph].mCodepoint, glyph );
    const auto segments = static_cast<u32>( mapped.size() + 1 );
    cmap.U16( 0 );
    cmap.U16( 1 );
    cmap.U16( 3 );
    cmap.U16( 1 );
    cmap.U32( 12 );
    cmap.U16( 4 );
    cmap.U16( 16 + segments * 8 );
    cmap.U16( 0 );
    cmap.U16( segments * 2 );
    u32 searchRange = 2;
    u32 entrySelector = 0;
    while ( searchRange * 2 <= segments * 2 )
    {
        searchRange *= 2;
        ++entrySelector;
    }
    cmap.U16( searchRange );
    cmap.U16( entrySelector );
    cmap.U16( segments * 2 - searchRange );
    for ( const auto& [code, glyph] : mapped )
        cmap.U16( code );
    cmap.U16( 0xFFFF );
    cmap.U16( 0 );
    for ( const auto& [code, glyph] : mapped )
        cmap.U16( code );
    cmap.U16( 0xFFFF );
    for ( const auto& [code, glyph] : mapped )
        cmap.U16( ( glyph - code ) & 0xFFFF );
    cmap.U16( 1 );
    for ( u32 segment = 0; segment < segments; ++segment )
        cmap.U16( 0 );

    FontWriter kern;
    kern.U16( 0 );
    kern.U16( 1 );
    kern.U16( 0 );
    kern.U16( 14 + 6 );
    kern.U16( 0x0001 );
    kern.U16( 1 );
    kern.U16( 6 );
    kern.U16( 0 );
    kern.U16( 0 );
    kern.U16( 2 );
    kern.U16( 3 );
    kern.I16( -100 );

    FontWriter name;
    name.U16( 0 );
    name.U16( 0 );
    name.U16( 6 );

    FontWriter post;
    post.U32( 0x00030000 );
    for ( u32 field = 0; field < 7; ++field )
        post.U32( 0 );

    // The tables in order of their tags, each from a four-byte boundary.
    const vector<pair<string, const FontWriter*>> tables{ { "cmap", &cmap }, { "glyf", &glyf }, { "head", &head },
                                                          { "hhea", &hhea }, { "hmtx", &hmtx }, { "kern", &kern },
                                                          { "loca", &loca }, { "maxp", &maxp }, { "name", &name },
                                                          { "post", &post } };
    FontWriter font;
    font.U32( 0x00010000 );
    font.U16( static_cast<u32>( tables.size() ) );
    font.U16( 128 );
    font.U16( 3 );
    font.U16( static_cast<u32>( tables.size() ) * 16 - 128 );
    u32 at = 12 + static_cast<u32>( tables.size() ) * 16;
    string data;
    for ( const auto& [tag, table] : tables )
    {
        string bytes = table->mBytes;
        while ( bytes.size() % 4 != 0 )
            bytes.push_back( '\0' );
        u32 sum = 0;
        for ( size_t word = 0; word < bytes.size(); word += 4 )
            sum += ( u32{ static_cast<u8>( bytes[word] ) } << 24 ) |
                   ( u32{ static_cast<u8>( bytes[word + 1] ) } << 16 ) |
                   ( u32{ static_cast<u8>( bytes[word + 2] ) } << 8 ) | u32{ static_cast<u8>( bytes[word + 3] ) };
        font.mBytes += tag;
        font.U32( sum );
        font.U32( at );
        font.U32( static_cast<u32>( table->mBytes.size() ) );
        at += static_cast<u32>( bytes.size() );
        data += bytes;
    }
    return font.mBytes + data;
}
}
