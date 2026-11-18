#include "bubble/data/png.hpp"
#include "bubble/types/algorithm.hpp"

namespace bubble
{
namespace
{
u32 Crc32( string_view bytes )
{
    u32 crc = ~0u;
    for ( const char c : bytes )
    {
        crc ^= static_cast<u8>( c );
        for ( u32 bit = 0; bit < 8; ++bit )
            crc = ( crc >> 1 ) ^ ( 0xEDB88320u & ( 0u - ( crc & 1u ) ) );
    }
    return ~crc;
}

void PutBigEndian( string& out, u32 value )
{
    for ( const u32 shift : { 24u, 16u, 8u, 0u } )
        out.push_back( static_cast<char>( ( value >> shift ) & 0xFF ) );
}

void PutChunk( string& out, string_view type, string_view data )
{
    PutBigEndian( out, static_cast<u32>( data.size() ) );
    const string typed = string( type ) + string( data );
    out += typed;
    PutBigEndian( out, Crc32( typed ) );
}
}

string EncodePng( span<const u8> rgba, ivec2 size )
{
    string raw;
    const size_t rowBytes = static_cast<size_t>( size.x ) * 4;
    for ( i32 row = 0; row < size.y; ++row )
    {
        // No filter.
        raw.push_back( 0 );
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
    PutBigEndian( deflated, ( b << 16 ) | a );

    string png( "\x89PNG\r\n\x1a\n", 8 );
    string header;
    PutBigEndian( header, static_cast<u32>( size.x ) );
    PutBigEndian( header, static_cast<u32>( size.y ) );
    // 8 bits, RGBA.
    header += string( "\x08\x06\x00\x00\x00", 5 );
    PutChunk( png, "IHDR", header );
    PutChunk( png, "IDAT", deflated );
    PutChunk( png, "IEND", "" );
    return png;
}
}
