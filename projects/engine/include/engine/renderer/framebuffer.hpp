#pragma once
#include "glm/glm.hpp"
#include <cstdint>
#include <cassert>
#include "engine/window/window.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/renderer/texture.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT FramebufferSpecification
{
    GLsizei mWidth = 0;
    GLsizei mHeight = 0;
    GLsizei mSamples = 1;
};


class BUBBLE_ENGINE_EXPORT Framebuffer
{
public:
    Framebuffer() = default;
    Framebuffer( const Framebuffer& ) = delete;
    Framebuffer& operator= ( const Framebuffer& ) = delete;

    Framebuffer( Framebuffer&& other ) noexcept;
    Framebuffer& operator = ( Framebuffer&& other ) noexcept;

    Framebuffer( const FramebufferSpecification& spec );
    Framebuffer( Texture2D&& color, Texture2D&& depth = Texture2D() );

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
    glm::uvec2 GetSize() const;
    void Resize( glm::uvec2 size );

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