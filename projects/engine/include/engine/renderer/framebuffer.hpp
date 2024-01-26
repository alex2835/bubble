#pragma once
#include <cstdint>
#include <cassert>
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"
#include "engine/window/window.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT FramebufferSpecification
{
    GLsizei mWidth = 0;
    GLsizei mHeight = 0;
    GLsizei mSamples = 1;

    uvec2 GetSize() const
    {
        return { mWidth, mHeight };
    }
};


class BUBBLE_ENGINE_EXPORT Framebuffer
{
public:
    Framebuffer() = delete;
    Framebuffer( const Framebuffer& ) = delete;
    Framebuffer& operator= ( const Framebuffer& ) = delete;

    Framebuffer( Framebuffer&& other ) noexcept;
    Framebuffer& operator = ( Framebuffer&& other ) noexcept;

    Framebuffer( const FramebufferSpecification& spec );
    Framebuffer( Texture2D&& color, Texture2D&& depth );

    void SetColorAttachment( Texture2D&& texture );
    void SetDepthAttachment( Texture2D&& texture );
    Texture2D GetColorAttachment();
    Texture2D GetDepthAttachment();

    virtual ~Framebuffer();

    void Bind() const;
    void Unbind() const;
    static void BindWindow( Window& window );
    void Invalidate();

    GLsizei GetWidth() const;
    GLsizei GetHeight() const;
    uvec2 GetSize() const;
    void Resize( uvec2 size );

    GLuint GetColorAttachmentRendererID() const;
    GLuint GetDepthAttachmentRendererID() const;
    FramebufferSpecification GetSpecification() const;


private:
    GLuint mRendererID = 0;
    Texture2D mColorAttachment;
    Texture2D mDepthAttachment;
    FramebufferSpecification mSpecification;
};
}