#include "engine/pch/pch.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/gpu_context.hpp"

namespace bubble
{
namespace
{

wgpu::AddressMode ToWGPU( AddressMode mode )
{
    switch ( mode )
    {
        case AddressMode::Repeat:       return wgpu::AddressMode::Repeat;
        case AddressMode::MirrorRepeat: return wgpu::AddressMode::MirrorRepeat;
        case AddressMode::ClampToEdge:  return wgpu::AddressMode::ClampToEdge;
    }
    BUBBLE_ASSERT( false, "Unknown AddressMode" );
    return wgpu::AddressMode::Repeat;
}

wgpu::FilterMode ToWGPU( FilterMode mode )
{
    switch ( mode )
    {
        case FilterMode::Nearest: return wgpu::FilterMode::Nearest;
        case FilterMode::Linear:  return wgpu::FilterMode::Linear;
    }
    BUBBLE_ASSERT( false, "Unknown FilterMode" );
    return wgpu::FilterMode::Linear;
}

}


u32 TextureFormatChannels( TextureFormat format )
{
    switch ( format )
    {
        case TextureFormat::R8Unorm:        return 1;
        case TextureFormat::RG8Unorm:       return 2;
        case TextureFormat::RGBA8Unorm:     return 4;
        case TextureFormat::RGBA8UnormSrgb: return 4;
        case TextureFormat::R32Uint:        return 1;
        case TextureFormat::Depth32Float:   return 1;
    }
    BUBBLE_ASSERT( false, "Unknown TextureFormat" );
    return 4;
}

u32 TextureFormatBytesPerPixel( TextureFormat format )
{
    switch ( format )
    {
        case TextureFormat::R8Unorm:        return 1;
        case TextureFormat::RG8Unorm:       return 2;
        case TextureFormat::RGBA8Unorm:     return 4;
        case TextureFormat::RGBA8UnormSrgb: return 4;
        case TextureFormat::R32Uint:        return 4;
        case TextureFormat::Depth32Float:   return 4;
    }
    BUBBLE_ASSERT( false, "Unknown TextureFormat" );
    return 4;
}

bool TextureFormatIsDepth( TextureFormat format )
{
    return format == TextureFormat::Depth32Float;
}

wgpu::TextureFormat ToWGPU( TextureFormat format )
{
    switch ( format )
    {
        case TextureFormat::R8Unorm:        return wgpu::TextureFormat::R8Unorm;
        case TextureFormat::RG8Unorm:       return wgpu::TextureFormat::RG8Unorm;
        case TextureFormat::RGBA8Unorm:     return wgpu::TextureFormat::RGBA8Unorm;
        case TextureFormat::RGBA8UnormSrgb: return wgpu::TextureFormat::RGBA8UnormSrgb;
        case TextureFormat::R32Uint:        return wgpu::TextureFormat::R32Uint;
        case TextureFormat::Depth32Float:   return wgpu::TextureFormat::Depth32Float;
    }
    BUBBLE_ASSERT( false, "Unknown TextureFormat" );
    return wgpu::TextureFormat::RGBA8Unorm;
}


void Texture2DSpecification::SetTextureSpecChanels( i32 channels )
{
    switch ( channels )
    {
        case 1:
            mFormat = TextureFormat::R8Unorm;
            break;
        case 2:
            mFormat = TextureFormat::RG8Unorm;
            break;
        // There is no rgb8unorm in WebGPU. The loader expands 3 channel images
        // to 4, so this is the correct storage format for them.
        case 3:
        case 4:
            mFormat = TextureFormat::RGBA8Unorm;
            break;
        default:
            BUBBLE_ASSERT( false, "Format not supported!" );
    }
}

u32 Texture2DSpecification::ExtractTextureSpecChannels() const
{
    return TextureFormatChannels( mFormat );
}

u32 Texture2DSpecification::GetTextureSize() const
{
    return mWidth * mHeight * TextureFormatBytesPerPixel( mFormat );
}

Texture2DSpecification Texture2DSpecification::CreateRGBA8( uvec2 size )
{
    return Texture2DSpecification( size );
}

Texture2DSpecification Texture2DSpecification::CreateObjectId( uvec2 size )
{
    Texture2DSpecification specification( size );
    specification.mFormat = TextureFormat::R32Uint;
    specification.mMinFilter = FilterMode::Nearest;
    specification.mMagFilter = FilterMode::Nearest;
    // WebGPU has no clamp-to-border, and the id buffer is only ever read at
    // exact pixel coordinates, so the edge behaviour is not observable.
    specification.mWrapS = AddressMode::ClampToEdge;
    specification.mWrapT = AddressMode::ClampToEdge;
    return specification;
}

Texture2DSpecification Texture2DSpecification::CreateDepth( uvec2 size )
{
    Texture2DSpecification specification( size );
    specification.mFormat = TextureFormat::Depth32Float;
    specification.mWrapS = AddressMode::ClampToEdge;
    specification.mWrapT = AddressMode::ClampToEdge;
    return specification;
}

Texture2DSpecification::Texture2DSpecification( uvec2 size )
    : mWidth( size.x ),
      mHeight( size.y )
{}


Texture2D::Texture2D( const Texture2DSpecification& spec )
    : mSpecification( spec )
{
    Invalidate();
}

Texture2D::Texture2D( Texture2D&& other ) noexcept
    : mSpecification( Texture2DSpecification::CreateRGBA8( {} ) )
{
    Swap( other );
}

Texture2D& Texture2D::operator=( Texture2D&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void Texture2D::Swap( Texture2D& other ) noexcept
{
    std::swap( mTexture, other.mTexture );
    std::swap( mView, other.mView );
    std::swap( mSampler, other.mSampler );
    std::swap( mSpecification, other.mSpecification );
}

void Texture2D::Invalidate()
{
    if ( mSpecification.mWidth == 0 or mSpecification.mHeight == 0 )
    {
        mTexture = {};
        mView = {};
        mSampler = {};
        return;
    }

    const bool isDepth = TextureFormatIsDepth( mSpecification.mFormat );

    wgpu::TextureDescriptor textureDesc = wgpu::Default;
    textureDesc.label = wgpu::StringView( "Texture2D" );
    textureDesc.dimension = wgpu::TextureDimension::_2D;
    textureDesc.size = { mSpecification.mWidth, mSpecification.mHeight, 1 };
    textureDesc.format = ToWGPU( mSpecification.mFormat );
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    // Usage has to be declared up front. TextureBinding so shaders can sample
    // it, RenderAttachment so it can be a framebuffer target, CopyDst for
    // SetData, and CopySrc so the id buffer can be read back.
    textureDesc.usage = wgpu::TextureUsage::TextureBinding |
                        wgpu::TextureUsage::RenderAttachment |
                        wgpu::TextureUsage::CopySrc |
                        ( isDepth ? wgpu::TextureUsage::None : wgpu::TextureUsage::CopyDst );
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;

    mTexture = wgpu::raii::Texture( Gpu().Device().createTexture( textureDesc ) );

    wgpu::TextureViewDescriptor viewDesc = wgpu::Default;
    viewDesc.label = wgpu::StringView( "Texture2D View" );
    viewDesc.format = textureDesc.format;
    viewDesc.dimension = wgpu::TextureViewDimension::_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = isDepth ? wgpu::TextureAspect::DepthOnly : wgpu::TextureAspect::All;

    mView = wgpu::raii::TextureView( mTexture->createView( viewDesc ) );

    wgpu::SamplerDescriptor samplerDesc = wgpu::Default;
    samplerDesc.label = wgpu::StringView( "Texture2D Sampler" );
    samplerDesc.addressModeU = ToWGPU( mSpecification.mWrapS );
    samplerDesc.addressModeV = ToWGPU( mSpecification.mWrapT );
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter = ToWGPU( mSpecification.mMagFilter );
    samplerDesc.minFilter = ToWGPU( mSpecification.mMinFilter );
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 1.0f;
    samplerDesc.maxAnisotropy = 1;
    // An integer or depth texture cannot use a filtering sampler.
    if ( isDepth or mSpecification.mFormat == TextureFormat::R32Uint )
    {
        samplerDesc.magFilter = wgpu::FilterMode::Nearest;
        samplerDesc.minFilter = wgpu::FilterMode::Nearest;
    }

    mSampler = wgpu::raii::Sampler( Gpu().Device().createSampler( samplerDesc ) );
}

void Texture2D::SetData( const void* data, u32 size )
{
    BUBBLE_ASSERT( mTexture, "SetData on a zero sized texture" );
    const u32 bytesPerPixel = TextureFormatBytesPerPixel( mSpecification.mFormat );
    [[maybe_unused]] const u32 expected = mSpecification.mWidth * mSpecification.mHeight * bytesPerPixel;
    BUBBLE_ASSERT( size == expected, "Data must be entire texture!" );

    wgpu::TexelCopyTextureInfo destination = wgpu::Default;
    destination.texture = *mTexture;
    destination.mipLevel = 0;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout source = wgpu::Default;
    source.offset = 0;
    source.bytesPerRow = mSpecification.mWidth * bytesPerPixel;
    source.rowsPerImage = mSpecification.mHeight;

    wgpu::Extent3D extent = { mSpecification.mWidth, mSpecification.mHeight, 1 };
    Gpu().Queue().writeTexture( destination, data, size, source, extent );
}

void Texture2D::Resize( const ivec2& new_size )
{
    mSpecification.mWidth = (u32)std::max( 0, new_size.x );
    mSpecification.mHeight = (u32)std::max( 0, new_size.y );
    Invalidate();
}

i32 Texture2D::Width() const
{
    return (i32)mSpecification.mWidth;
}

i32 Texture2D::Height() const
{
    return (i32)mSpecification.mHeight;
}

const Texture2DSpecification& Texture2D::Specification() const
{
    return mSpecification;
}

u64 Texture2D::ImTextureId() const
{
    if ( not mView )
        return 0;
    return (u64)(uintptr_t)(WGPUTextureView)( *mView );
}

bool Texture2D::operator==( const Texture2D& other ) const
{
    return mTexture and other.mTexture and
           (WGPUTexture)( *mTexture ) == (WGPUTexture)( *other.mTexture );
}

}
