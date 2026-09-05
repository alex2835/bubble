#pragma once
#include "engine/utils/error.hpp"
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"
#include "engine/renderer/webgpu.hpp"

namespace bubble
{

// Replaces the raw GL enums this struct used to hold. Nothing here was ever
// serialized - every texture's format is re-derived at load time from the image
// file's channel count, or comes from one of the factories below - so switching
// to an abstract enum breaks no saved project.
enum class TextureFormat
{
    R8Unorm,
    RG8Unorm,
    RGBA8Unorm,
    RGBA8UnormSrgb,
    // Single channel unsigned integer, for the entity id buffer.
    R32Uint,
    Depth32Float,
};

enum class FilterMode
{
    Nearest,
    Linear,
};

enum class AddressMode
{
    Repeat,
    MirrorRepeat,
    ClampToEdge,
};

u32 TextureFormatChannels( TextureFormat format );
u32 TextureFormatBytesPerPixel( TextureFormat format );
bool TextureFormatIsDepth( TextureFormat format );
wgpu::TextureFormat ToWGPU( TextureFormat format );


struct Texture2DSpecification
{
    u32 mWidth = 0;
    u32 mHeight = 0;
    TextureFormat mFormat = TextureFormat::RGBA8Unorm;
    FilterMode mMinFilter = FilterMode::Linear;
    FilterMode mMagFilter = FilterMode::Linear;
    AddressMode mWrapS = AddressMode::Repeat;
    AddressMode mWrapT = AddressMode::Repeat;
    bool mFlip = false;
    bool mMipmaps = false;

    Texture2DSpecification( const Texture2DSpecification& ) = default;

    // WebGPU has no three channel format, so a 3 channel image is stored as
    // RGBA and the loader expands it. See texture_loader.
    void SetTextureSpecChanels( i32 channels );
    u32 ExtractTextureSpecChannels() const;
    u32 GetTextureSize() const;

    static Texture2DSpecification CreateRGBA8( uvec2 size );
    static Texture2DSpecification CreateObjectId( uvec2 size );
    static Texture2DSpecification CreateDepth( uvec2 size );

private:
    Texture2DSpecification( uvec2 size );
};


class Texture2D
{
public:
    Texture2D( const Texture2DSpecification& spec );
    Texture2D( const Texture2D& ) = delete;
    Texture2D& operator= ( const Texture2D& ) = delete;
    Texture2D( Texture2D&& ) noexcept;
    Texture2D& operator= ( Texture2D&& ) noexcept;
    ~Texture2D() = default;

    void Swap( Texture2D& other ) noexcept;

    i32 Width()  const;
    i32 Height() const;
    const Texture2DSpecification& Specification() const;

    // Uploads the whole texture. Size is in bytes and must match exactly.
    void SetData( const void* data, u32 size );

    void Resize( const ivec2& new_size );
    void Invalidate();

    wgpu::Texture GetTexture() const { return *mTexture; }
    wgpu::TextureView View() const { return *mView; }
    wgpu::Sampler Sampler() const { return *mSampler; }

    // The handle ImGui::Image wants. Cast to ImTextureID at the call site; this
    // header deliberately does not pull in imgui.h.
    u64 ImTextureId() const;

    bool operator==( const Texture2D& other ) const;

public:
    string mName;
    path mPath;

private:
    wgpu::raii::Texture mTexture;
    wgpu::raii::TextureView mView;
    wgpu::raii::Sampler mSampler;
    Texture2DSpecification mSpecification;
};

}
