#pragma once
#include <cstdint>
#include <cassert>
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/utility.hpp"
#include "engine/window/window.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{
struct FramebufferSpecification
{
    i32 mWidth = 0;
    i32 mHeight = 0;
    i32 mSamples = 1;

    uvec2 Size() const
    {
        return { mWidth, mHeight };
    }
};


class Framebuffer
{
public:
    Framebuffer() = delete;
    Framebuffer( const Framebuffer& ) = delete;
    Framebuffer& operator= ( const Framebuffer& ) = delete;
    Framebuffer( Framebuffer&& other ) noexcept;
    Framebuffer& operator = ( Framebuffer&& other ) noexcept;
    Framebuffer( Texture2D&& color, Texture2D&& depth );
    ~Framebuffer();

    void Swap( Framebuffer& other ) noexcept;

    void SetColorAttachment( Texture2D&& texture );
    void SetDepthAttachment( Texture2D&& texture );
    void SetStencilAttachment( Texture2D&& texture );
    Texture2D& ColorAttachment();
    Texture2D& DepthAttachment();
    opt<Texture2D>& StencilAttachment();

    u32 ReadColorAttachmentPixelRedUint( uvec2 pos );
    
    void Bind() const;
    void Unbind() const;
    static void BindWindow( Window& window );
    void Invalidate();

    i32 Width() const;
    i32 Height() const;
    uvec2 Size() const;
    void Resize( uvec2 size );

    FramebufferSpecification Specification() const;

private:
    u32 mRendererID = 0;
    Texture2D mColorAttachment;
    Texture2D mDepthAttachment;
    opt<Texture2D> mStencilAttachment;
    FramebufferSpecification mSpecification;
};
}