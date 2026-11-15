#include "bubble/data/binary.hpp"

namespace bubble
{
void BinaryWriter::U32( u32 value )
{
    for ( u32 shift = 0; shift < 32; shift += 8 )
        mBytes.push_back( static_cast<char>( ( value >> shift ) & 0xff ) );
}

void BinaryWriter::F32( f32 value )
{
    U32( bit_cast<u32>( value ) );
}

void BinaryWriter::Bytes( string_view bytes )
{
    mBytes.append( bytes );
}

void BinaryWriter::Bytes( span<const u8> bytes )
{
    mBytes.append( bytes.begin(), bytes.end() );
}

void BinaryWriter::Text( string_view text )
{
    U32( static_cast<u32>( text.size() ) );
    Bytes( text );
}

opt<u32> BinaryReader::TryU32()
{
    if ( Left() < 4 )
        return nullopt;
    u32 value = 0;
    for ( u32 i = 0; i < 4; ++i )
        value |= u32{ static_cast<u8>( mBytes[mAt + i] ) } << ( 8 * i );
    mAt += 4;
    return value;
}

opt<f32> BinaryReader::TryF32()
{
    const auto bitsMaybe = TryU32();
    if ( not bitsMaybe )
        return nullopt;
    return bit_cast<f32>( *bitsMaybe );
}

opt<string_view> BinaryReader::TryBytes( u64 count )
{
    if ( Left() < count )
        return nullopt;
    const string_view taken = mBytes.substr( mAt, static_cast<size_t>( count ) );
    mAt += static_cast<size_t>( count );
    return taken;
}

opt<string_view> BinaryReader::TryText()
{
    const auto lengthMaybe = TryU32();
    if ( not lengthMaybe )
        return nullopt;
    return TryBytes( *lengthMaybe );
}
}
