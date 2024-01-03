
#include "engine/log/log.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{
Framebuffer::Framebuffer( const FramebufferSpecification& spec )
    : mSpecification( spec ),
      mColorAttachment( Texture2DSpecification::CreateRGBA8() ),
      mDepthAttachment( Texture2DSpecification::CreateDepth() )
{
    Invalidate();
}

Framebuffer::Framebuffer( Texture2D&& color, Texture2D&& depth )
    : mColorAttachment( std::move( color ) ),
      mDepthAttachment( std::move( depth ) )
{
    mSpecification = { mColorAttachment.GetWidth(), mColorAttachment.GetHeight() };
    Invalidate();
}

Framebuffer::Framebuffer( Framebuffer&& other ) noexcept
{
    mRendererID = other.mRendererID;
    mSpecification = other.mSpecification;
    mColorAttachment = std::move( other.mColorAttachment );
    mDepthAttachment = std::move( other.mDepthAttachment );
    other.mRendererID = 0;
}

Framebuffer& Framebuffer::operator= ( Framebuffer&& other ) noexcept
{
    if( this != &other )
    {
        glcall( glDeleteFramebuffers( 1, &mRendererID ) );
        mRendererID = other.mRendererID;
        mSpecification = other.mSpecification;
        mColorAttachment = std::move( other.mColorAttachment );
        mDepthAttachment = std::move( other.mDepthAttachment );
        other.mRendererID = 0;
    }
    return *this;
}

void Framebuffer::SetColorAttachment( Texture2D&& texture )
{
    mColorAttachment = std::move( texture );
}

void Framebuffer::SetDepthAttachment( Texture2D&& texture )
{
    mDepthAttachment = std::move( texture );
}

Texture2D Framebuffer::GetColorAttachment()
{
    return std::move( mColorAttachment );
}

Texture2D Framebuffer::GetDepthAttachment()
{
    return std::move( mDepthAttachment );
}

Framebuffer::~Framebuffer()
{
    glcall( glDeleteFramebuffers( 1, &mRendererID ) );
}

void Framebuffer::Invalidate()
{
    glcall( glDeleteFramebuffers( 1, &mRendererID ) );
    glcall( glGenFramebuffers( 1, &mRendererID ) );
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, mRendererID ) );

    if( mColorAttachment.GetWidth() != mSpecification.mWidth ||
        mColorAttachment.GetHeight() != mSpecification.mHeight )
    {
        mColorAttachment.Resize( { mSpecification.mWidth, mSpecification.mHeight } );
        mDepthAttachment.Resize( { mSpecification.mWidth, mSpecification.mHeight } );
    }

    glcall( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GetColorAttachmentRendererID(), 0 ) );
    glcall( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, GetDepthAttachmentRendererID(), 0 ) );

    BUBBLE_ASSERT( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!" );
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
}

GLsizei Framebuffer::GetWidth() const
{
    return mSpecification.mWidth;
}

GLsizei Framebuffer::GetHeight() const
{
    return mSpecification.mHeight;
}

void Framebuffer::Bind() const
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, mRendererID ) );
    glViewport( 0, 0, GetWidth(), GetHeight() );
}

void Framebuffer::Unbind() const
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
}

void Framebuffer::BindWindow( Window& window )
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    WindowSize window_size = window.GetSize();
    glViewport( 0, 0, window_size.mWidth, window_size.mHeight );
}

glm::uvec2 Framebuffer::GetSize() const
{
    return { GetWidth(), GetHeight() };
}

void Framebuffer::Resize( glm::uvec2 size )
{
    // Prevent framebuffer error
    mSpecification.mWidth = std::max( 1u, size.x );
    mSpecification.mHeight = std::max( 1u, size.y );
    Invalidate();
}

FramebufferSpecification Framebuffer::GetSpecification() const
{
    return mSpecification;
}

GLuint Framebuffer::GetColorAttachmentRendererID() const
{
    return mColorAttachment.GetRendererID();
}

GLuint Framebuffer::GetDepthAttachmentRendererID() const
{
    return mDepthAttachment.GetRendererID();
}

}

