// A picture checked against the one kept in the repository: PNG written -
// with no compression, which is all a reference needs - and read back by
// stb_image, and two pictures compared with what a rasterizer may differ by.
// Run the tests with BUBBLE_UPDATE_REFERENCES set to write the references
// anew; a picture that fails is written beside the tests' binary.
#pragma once
#include "bubble/core/utf8.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/stream.hpp"
#include <cstdlib>
#include <doctest.h>
#include <stb_image.h>

namespace bubble::test
{
namespace detail
{
inline u32 Crc32( string_view bytes, u32 crc = 0 )
{
    crc = ~crc;
    for ( const char c : bytes )
    {
        crc ^= static_cast<u8>( c );
        for ( u32 bit = 0; bit < 8; ++bit )
            crc = ( crc >> 1 ) ^ ( 0xEDB88320u & ( 0u - ( crc & 1u ) ) );
    }
    return ~crc;
}

inline void PutBigEndian( string& out, u32 value )
{
    for ( const u32 shift : { 24u, 16u, 8u, 0u } )
        out.push_back( static_cast<char>( ( value >> shift ) & 0xFF ) );
}

inline void PutChunk( string& out, string_view type, string_view data )
{
    PutBigEndian( out, static_cast<u32>( data.size() ) );
    const string typed = string( type ) + string( data );
    out += typed;
    PutBigEndian( out, Crc32( typed ) );
}
}

// RGBA8 pixels, rows from the top, as a PNG: zlib's stored blocks, no
// compression.
inline string EncodePng( const vector<u8>& rgba, ivec2 size )
{
    string raw;
    const size_t rowBytes = static_cast<size_t>( size.x ) * 4;
    for ( i32 row = 0; row < size.y; ++row )
    {
        raw.push_back( 0 ); // no filter
        raw.append( reinterpret_cast<const char*>( rgba.data() ) + rowBytes * static_cast<size_t>( row ), rowBytes );
    }
    string deflated{ '\x78', '\x01' };
    u32 a = 1;
    u32 b = 0;
    for ( const char c : raw )
    {
        a = ( a + static_cast<u8>( c ) ) % 65521;
        b = ( b + a ) % 65521;
    }
    for ( size_t at = 0; at < raw.size() or at == 0; at += 65535 )
    {
        const size_t length = min<size_t>( 65535, raw.size() - at );
        deflated.push_back( at + length >= raw.size() ? 1 : 0 );
        deflated.push_back( static_cast<char>( length & 0xFF ) );
        deflated.push_back( static_cast<char>( length >> 8 ) );
        deflated.push_back( static_cast<char>( ~length & 0xFF ) );
        deflated.push_back( static_cast<char>( ( ~length >> 8 ) & 0xFF ) );
        deflated.append( raw, at, length );
    }
    detail::PutBigEndian( deflated, ( b << 16 ) | a );

    string png( "\x89PNG\r\n\x1a\n", 8 );
    string header;
    detail::PutBigEndian( header, static_cast<u32>( size.x ) );
    detail::PutBigEndian( header, static_cast<u32>( size.y ) );
    header += string( "\x08\x06\x00\x00\x00", 5 ); // 8 bits, RGBA
    detail::PutChunk( png, "IHDR", header );
    detail::PutChunk( png, "IDAT", deflated );
    detail::PutChunk( png, "IEND", "" );
    return png;
}

// How far two pictures of one size are apart: the mean difference of a
// channel, in levels, and the share of pixels with a channel off by more
// than `far` levels.
struct ImageDifference
{
    f32 mMean = 0;
    f32 mFarShare = 0;
};

inline ImageDifference Compare( const vector<u8>& a, const vector<u8>& b, u32 far )
{
    ImageDifference difference;
    u64 total = 0;
    size_t farPixels = 0;
    for ( size_t pixel = 0; pixel * 4 < a.size(); ++pixel )
    {
        bool off = false;
        for ( size_t channel = 0; channel < 3; ++channel )
        {
            const u32 apart = static_cast<u32>( abs( a[pixel * 4 + channel] - b[pixel * 4 + channel] ) );
            total += apart;
            off = off or apart > far;
        }
        farPixels += off ? 1 : 0;
    }
    const size_t pixels = a.size() / 4;
    difference.mMean = static_cast<f32>( total ) / static_cast<f32>( pixels * 3 );
    difference.mFarShare = static_cast<f32>( farPixels ) / static_cast<f32>( pixels );
    return difference;
}

inline string ReadFile( const OsPath& path )
{
    ifstream file( path, ios::binary );
    ostringstream bytes;
    bytes << file.rdbuf();
    return std::move( bytes ).str();
}

// Checks `rgba` against the reference `name` in tests/render/reference: a
// rasterizer's own rounding is let pass - a mean within `mean` levels, at
// most `farShare` of the pixels off by more than `far` - and nothing more.
inline void CheckAgainstReference( const vector<u8>& rgba,
                                   ivec2 size,
                                   string_view name,
                                   f32 mean = 2.0f,
                                   u32 far = 24,
                                   f32 farShare = 0.01f )
{
    const OsPath reference = PathFromUtf8( format( "{}/render/reference/{}.png", BUBBLE_TEST_SOURCE_DIR, name ) );
    if ( std::getenv( "BUBBLE_UPDATE_REFERENCES" ) != nullptr )
    {
        ofstream( reference, ios::binary ) << EncodePng( rgba, size );
        MESSAGE( format( "wrote the reference {}", name ) );
        return;
    }
    const string bytes = ReadFile( reference );
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    u8* decoded = stbi_load_from_memory( reinterpret_cast<const u8*>( bytes.data() ), static_cast<i32>( bytes.size() ),
                                         &width, &height, &channels, 4 );
    if ( decoded == nullptr )
        FAIL( format( "no reference {} to compare with: run the tests with BUBBLE_UPDATE_REFERENCES set", name ) );
    const vector<u8> expected( decoded, decoded + static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4 );
    stbi_image_free( decoded );
    REQUIRE( ivec2( width, height ) == size );

    const ImageDifference difference = Compare( rgba, expected, far );
    if ( difference.mMean > mean or difference.mFarShare > farShare )
    {
        const string actual = format( "{}/{}.actual.png", BUBBLE_TEST_BINARY_DIR, name );
        ofstream( PathFromUtf8( actual ), ios::binary ) << EncodePng( rgba, size );
        FAIL( format( "{} is {:.2f} levels off on the mean, {:.2f}% of its pixels more than {} - drawn as {}", name,
                      difference.mMean, difference.mFarShare * 100.0f, far, actual ) );
    }
}
}
