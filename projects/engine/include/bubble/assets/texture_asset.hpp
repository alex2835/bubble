#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class AssetRegistry;

// One level of a texture: RGBA, 8 bits each, rows from the top.
struct TextureMip
{
    u32 mWidth = 0;
    u32 mHeight = 0;
    vector<u8> mPixels;
};

// An image as the importer made it: every mip level, the full size first,
// down to 1×1 - or the full size alone. For now plain RGBA8; GPU-compressed
// blocks (Basis in KTX2) come with the renderer, as a new importer version
// and this read differently.
struct TextureAsset
{
    // Colors stored as sRGB, as an image a person paints; false for data -
    // normals, masks - stored as they are.
    bool mSrgb = true;
    vector<TextureMip> mMips;
};

constexpr string_view cTextureKind = "texture";

// The bytes the importer writes and the loader reads: a header - "BTEX",
// version, flags, levels - then each level's size and pixels, little-endian.
string WriteTextureAsset( const TextureAsset& texture );
// The error says what is wrong: cut short, another version, a level whose
// size does not fit its pixels.
expected<TextureAsset, string> TryReadTextureAsset( string_view bytes );

// Teaches the registry textures: <id>.texture.
void RegisterTextureLoader( AssetRegistry& registry );
}
