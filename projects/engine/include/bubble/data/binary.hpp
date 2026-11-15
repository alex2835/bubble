#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// The bytes of what an importer makes - a texture, a model - written and
// read the same everywhere: little-endian, whatever the machine.
class BinaryWriter
{
public:
    void U32( u32 value );
    void F32( f32 value );
    void Bytes( string_view bytes );
    void Bytes( span<const u8> bytes );
    // Its length first, then the text.
    void Text( string_view text );

    string Take() { return std::move( mBytes ); }

private:
    string mBytes;
};

// Reads from the front, refusing to go past the end: each read is nothing
// when the bytes run out.
class BinaryReader
{
public:
    explicit BinaryReader( string_view bytes ) : mBytes( bytes ) {}

    opt<u32> TryU32();
    opt<f32> TryF32();
    opt<string_view> TryBytes( u64 count );
    opt<string_view> TryText();
    // What is left, for a count that has to fit in it.
    u64 Left() const { return mBytes.size() - mAt; }
    bool AtEnd() const { return mAt == mBytes.size(); }

private:
    string_view mBytes;
    size_t mAt = 0;
};
}
