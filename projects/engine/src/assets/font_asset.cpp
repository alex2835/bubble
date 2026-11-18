#include "bubble/assets/font_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
constexpr string_view cMagic = "BFNT";
constexpr u32 cVersion = 1;
// A glyph: its codepoint, advance and two boxes.
constexpr u64 cGlyphBytes = 10 * 4;
constexpr u64 cKerningBytes = 3 * 4;

void WriteVec4( BinaryWriter& out, vec4 v )
{
    for ( i32 i = 0; i < 4; ++i )
        out.F32( v[i] );
}

vec4 ReadVec4( BinaryReader& reader )
{
    vec4 v( 0.0f );
    for ( i32 i = 0; i < 4; ++i )
        v[i] = *reader.TryF32();
    return v;
}
}

OptRef<const FontGlyph> FontAsset::TryGlyph( u32 codepoint ) const
{
    const auto found = lower_bound( mGlyphs.begin(), mGlyphs.end(), codepoint,
                                    []( const FontGlyph& glyph, u32 wanted ) { return glyph.mCodepoint < wanted; } );
    if ( found == mGlyphs.end() or found->mCodepoint != codepoint )
        return nullopt;
    return *found;
}

f32 FontAsset::Kerning( u32 first, u32 second ) const
{
    const auto found = lower_bound( mKerning.begin(), mKerning.end(), pair<u32, u32>( first, second ),
                                    []( const FontKerning& kerning, const pair<u32, u32>& wanted ) {
                                        return pair<u32, u32>( kerning.mFirst, kerning.mSecond ) < wanted;
                                    } );
    if ( found == mKerning.end() or found->mFirst != first or found->mSecond != second )
        return 0;
    return found->mAdvance;
}

string WriteFontAsset( const FontAsset& font )
{
    BinaryWriter out;
    out.Bytes( cMagic );
    out.U32( cVersion );
    out.F32( font.mSize );
    out.F32( font.mRange );
    out.F32( font.mAscender );
    out.F32( font.mDescender );
    out.F32( font.mLineHeight );
    out.U32( static_cast<u32>( font.mGlyphs.size() ) );
    for ( const FontGlyph& glyph : font.mGlyphs )
    {
        out.U32( glyph.mCodepoint );
        out.F32( glyph.mAdvance );
        WriteVec4( out, glyph.mPlane );
        WriteVec4( out, glyph.mAtlas );
    }
    out.U32( static_cast<u32>( font.mKerning.size() ) );
    for ( const FontKerning& kerning : font.mKerning )
    {
        out.U32( kerning.mFirst );
        out.U32( kerning.mSecond );
        out.F32( kerning.mAdvance );
    }
    out.Text( WriteTextureAsset( font.mAtlas ) );
    return out.Take();
}

expected<FontAsset, string> TryReadFontAsset( string_view bytes )
{
    BinaryReader reader( bytes );
    const auto cutShort = [] { return unexpected( "a font cut short"s ); };
    const auto magicMaybe = reader.TryBytes( cMagic.size() );
    if ( not magicMaybe or *magicMaybe != cMagic )
        return unexpected( "not a font the importer made"s );
    const auto versionMaybe = reader.TryU32();
    if ( not versionMaybe )
        return cutShort();
    if ( *versionMaybe != cVersion )
        return unexpected( format( "a font of version {}, not {}: import it again", *versionMaybe, cVersion ) );

    FontAsset font;
    for ( f32* metric : { &font.mSize, &font.mRange, &font.mAscender, &font.mDescender, &font.mLineHeight } )
    {
        const auto valueMaybe = reader.TryF32();
        if ( not valueMaybe )
            return cutShort();
        *metric = *valueMaybe;
    }
    const auto glyphsMaybe = reader.TryU32();
    if ( not glyphsMaybe or *glyphsMaybe * cGlyphBytes > reader.Left() )
        return cutShort();
    font.mGlyphs.resize( *glyphsMaybe );
    for ( FontGlyph& glyph : font.mGlyphs )
    {
        glyph.mCodepoint = *reader.TryU32();
        glyph.mAdvance = *reader.TryF32();
        glyph.mPlane = ReadVec4( reader );
        glyph.mAtlas = ReadVec4( reader );
    }
    for ( size_t g = 1; g < font.mGlyphs.size(); ++g )
        if ( font.mGlyphs[g - 1].mCodepoint >= font.mGlyphs[g].mCodepoint )
            return unexpected( format( "a font's glyphs out of order at U+{:04X}", font.mGlyphs[g].mCodepoint ) );
    const auto kerningMaybe = reader.TryU32();
    if ( not kerningMaybe or *kerningMaybe * cKerningBytes > reader.Left() )
        return cutShort();
    font.mKerning.resize( *kerningMaybe );
    for ( FontKerning& kerning : font.mKerning )
    {
        kerning.mFirst = *reader.TryU32();
        kerning.mSecond = *reader.TryU32();
        kerning.mAdvance = *reader.TryF32();
    }
    const auto atlasBytesMaybe = reader.TryText();
    if ( not atlasBytesMaybe )
        return cutShort();
    auto atlasMaybe = TryReadTextureAsset( *atlasBytesMaybe );
    if ( not atlasMaybe )
        return unexpected( "a font's atlas: " + atlasMaybe.error() );
    font.mAtlas = std::move( *atlasMaybe );
    if ( font.mAtlas.mMips.empty() )
        return unexpected( "a font with an empty atlas"s );
    if ( not reader.AtEnd() )
        return unexpected( "a font with more after its atlas"s );
    return font;
}

void RegisterFontLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<FontAsset>(
        cFontKind, []( string_view bytes, const AssetPath& ) { return TryReadFontAsset( bytes ); } );
}
}
