#pragma once
#include "bubble/render/gpu/gpu_handle.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
class Gpu;
struct TextureAsset;

struct GpuTextureOptions
{
    ivec2 mSize{ 1 };
    u32 mMips = 1;
    WGPUTextureFormat mFormat = WGPUTextureFormat_RGBA8UnormSrgb;
    // What it is for: sampled, drawn into, copied from or to.
    WGPUTextureUsage mUsage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    string_view mLabel;
};

// A 2D texture on the GPU and the view of all its levels, which is what
// shaders sample and passes draw into.
class GpuTexture
{
public:
    GpuTexture() = default;
    GpuTexture( const Gpu& gpu, const GpuTextureOptions& options );
    // An imported image with every level it has, for sampling: colors as
    // sRGB, data as it is, as it was imported. One with no levels is a
    // logic_error - the loader never makes one.
    GpuTexture( const Gpu& gpu, const TextureAsset& texture, string_view label );

    // One level's texels, rows from the top, packed: 4 bytes a texel, the
    // formats it takes. A texture made with CopyDst; another size, level or
    // format is a logic_error.
    void Write( const Gpu& gpu, u32 mip, span<const byte> texels ) const;

    WGPUTexture Get() const { return mTexture.Get(); }
    WGPUTextureView View() const { return mView.Get(); }
    ivec2 Size() const { return mSize; }
    // A level's size: half the one before, 1 at least. A level it does not
    // have is a logic_error.
    ivec2 MipSize( u32 mip ) const;
    u32 Mips() const { return mMips; }
    WGPUTextureFormat Format() const { return mFormat; }
    explicit operator bool() const { return static_cast<bool>( mTexture ); }

private:
    GpuTextureHandle mTexture;
    GpuTextureViewHandle mView;
    ivec2 mSize{ 0 };
    u32 mMips = 0;
    WGPUTextureFormat mFormat = WGPUTextureFormat_Undefined;
};
}
