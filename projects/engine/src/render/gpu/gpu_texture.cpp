#include "bubble/render/gpu/gpu_texture.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
u32 TexelBytes( WGPUTextureFormat format )
{
    switch ( format )
    {
        case WGPUTextureFormat_RGBA8Unorm:
        case WGPUTextureFormat_RGBA8UnormSrgb:
        case WGPUTextureFormat_BGRA8Unorm:
        case WGPUTextureFormat_BGRA8UnormSrgb: return 4;
        default: throw logic_error( "GpuTexture::Write knows 4-byte texels only" );
    }
}

GpuTextureOptions OptionsFor( const TextureAsset& texture, string_view label )
{
    if ( texture.mMips.empty() )
        throw logic_error( format( "GpuTexture of {}: an image with no levels", label ) );
    const TextureMip& full = texture.mMips.front();
    return GpuTextureOptions{
        .mSize = { static_cast<i32>( full.mWidth ), static_cast<i32>( full.mHeight ) },
        .mMips = static_cast<u32>( texture.mMips.size() ),
        .mFormat = texture.mSrgb ? WGPUTextureFormat_RGBA8UnormSrgb : WGPUTextureFormat_RGBA8Unorm,
        .mUsage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .mLabel = label,
    };
}
}

GpuTexture::GpuTexture( const Gpu& gpu, const GpuTextureOptions& options )
    : mSize( options.mSize ),
      mMips( options.mMips ),
      mFormat( options.mFormat )
{
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( options.mLabel );
    descriptor.usage = options.mUsage;
    descriptor.size = WGPUExtent3D{ static_cast<u32>( mSize.x ), static_cast<u32>( mSize.y ), 1 };
    descriptor.format = mFormat;
    descriptor.mipLevelCount = mMips;
    mTexture = GpuTextureHandle( wgpuDeviceCreateTexture( gpu.Device(), &descriptor ) );
    mView = GpuTextureViewHandle( wgpuTextureCreateView( mTexture.Get(), nullptr ) );
}

GpuTexture::GpuTexture( const Gpu& gpu, const TextureAsset& texture, string_view label )
    : GpuTexture( gpu, OptionsFor( texture, label ) )
{
    for ( u32 mip = 0; mip < mMips; ++mip )
        Write( gpu, mip, AsBytes( texture.mMips[mip].mPixels ) );
}

ivec2 GpuTexture::MipSize( u32 mip ) const
{
    if ( mip >= mMips )
        throw logic_error( format( "GpuTexture::MipSize of level {}: it has {}", mip, mMips ) );
    return ivec2( max( mSize.x >> mip, 1 ), max( mSize.y >> mip, 1 ) );
}

void GpuTexture::Write( const Gpu& gpu, u32 mip, span<const byte> texels ) const
{
    const ivec2 size = MipSize( mip );
    const u32 rowBytes = static_cast<u32>( size.x ) * TexelBytes( mFormat );
    if ( texels.size() != size_t{ rowBytes } * static_cast<u32>( size.y ) )
        throw logic_error( format( "GpuTexture::Write of {} bytes to level {}: it holds {}x{} texels there",
                                   texels.size(), mip, size.x, size.y ) );
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = mTexture.Get();
    destination.mipLevel = mip;
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = rowBytes;
    layout.rowsPerImage = static_cast<u32>( size.y );
    const WGPUExtent3D extent{ static_cast<u32>( size.x ), static_cast<u32>( size.y ), 1 };
    wgpuQueueWriteTexture( gpu.Queue(), &destination, texels.data(), texels.size(), &layout, &extent );
}
}
