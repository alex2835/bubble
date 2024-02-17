
#include "engine/log/log.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{
void ResizeAttachment( Texture2D& attachment, uvec2 size )
{
    if ( attachment.Width() != size.x || attachment.Height() != size.y )
        attachment.Resize( size );
}

Framebuffer::Framebuffer( Texture2D&& color, Texture2D&& depth )
    : mColorAttachment( std::move( color ) ),
    mDepthAttachment( std::move( depth ) )
{
    mSpecification = { mColorAttachment.Width(), mColorAttachment.Height() };
    Invalidate();
}

Framebuffer::Framebuffer( Framebuffer&& other ) noexcept
    : mColorAttachment( std::move( other.mColorAttachment ) ),
    mDepthAttachment( std::move( other.mDepthAttachment ) )
{
    mRendererID = other.mRendererID;
    mSpecification = other.mSpecification;
    other.mRendererID = 0;
}

Framebuffer& Framebuffer::operator= ( Framebuffer&& other ) noexcept
{
    if ( this != &other )
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

Texture2D& Framebuffer::ColorAttachment()
{
    return mColorAttachment;
}

Texture2D& Framebuffer::DepthAttachment()
{
    return mDepthAttachment;
}

opt<Texture2D>& Framebuffer::StencilAttachment()
{
    return mStencilAttachment;
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


    auto size = uvec2( mSpecification.mWidth, mSpecification.mHeight );
    ResizeAttachment( mColorAttachment, size );
    ResizeAttachment( mDepthAttachment, size );
    if ( mStencilAttachment )
        ResizeAttachment( *mStencilAttachment, size );

    glcall( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorAttachment.RendererID(), 0 ) );
    glcall( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthAttachment.RendererID(), 0 ) );
    if ( mStencilAttachment )
        glcall( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, mStencilAttachment->RendererID(), 0 ) );

    BUBBLE_ASSERT( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!" );
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
}

GLsizei Framebuffer::Width() const
{
    return mSpecification.mWidth;
}

GLsizei Framebuffer::Height() const
{
    return mSpecification.mHeight;
}

void Framebuffer::Bind() const
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, mRendererID ) );
    glViewport( 0, 0, Width(), Height() );
}

void Framebuffer::Unbind() const
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
}

void Framebuffer::BindWindow( Window& window )
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    WindowSize window_size = window.Size();
    glViewport( 0, 0, window_size.mWidth, window_size.mHeight );
}

uvec2 Framebuffer::Size() const
{
    return { Width(), Height() };
}

void Framebuffer::Resize( uvec2 size )
{
    // Prevent framebuffer error
    mSpecification.mWidth = std::max( 1u, size.x );
    mSpecification.mHeight = std::max( 1u, size.y );
    Invalidate();
}

FramebufferSpecification Framebuffer::Specification() const
{
    return mSpecification;
}

}