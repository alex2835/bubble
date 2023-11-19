#pragma once
#include "glm/glm.hpp"
#include <cstdint>
#include <cassert>
#include "window/window.hpp"
#include "utils/imexp.hpp"
#include "renderer/texture.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT FramebufferSpecification
{
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    int mSamples = 1;
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

    int GetWidth() const;
    int GetHeight() const;
    glm::ivec2 GetSize() const;
    void Resize( glm::ivec2 size );
    void Resize( uint32_t width, uint32_t height );

    GLuint GetColorAttachmentRendererID() const;
    GLuint GetDepthAttachmentRendererID() const;
    FramebufferSpecification GetSpecification() const;


private:
    static Texture2DSpecification GetDepthAttachemtSpec();

    GLuint mRendererID = 0;
    Texture2D mColorAttachment;
    Texture2D mDepthAttachment;
    FramebufferSpecification mSpecification;
};
}