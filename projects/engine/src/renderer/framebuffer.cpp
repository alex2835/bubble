#include "engine/pch/pch.hpp"
#include <GL/glew.h>
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
    : mColorAttachment( Texture2DSpecification::CreateRGBA8( {} ) ),
      mDepthAttachment( Texture2DSpecification::CreateDepth( {} ) )
{
    Swap( other );
}

Framebuffer& Framebuffer::operator= ( Framebuffer&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
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

u32 Framebuffer::ReadColorAttachmentPixelRedUint( uvec2 pos )
{
    u32 pixel = 0;
    Bind();
    glcall( glReadBuffer( GL_COLOR_ATTACHMENT0 ) );
    glcall( glReadPixels( pos.x, pos.y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixel ) );
    glcall( glReadBuffer( GL_NONE ) );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    Unbind();

    return pixel;
}

const vector<u32>& Framebuffer::ReadColorAttachmentPixelRedUint( uvec2 start, uvec2 end )
{
    auto x = std::min( start.x, end.x );
    auto y = std::min( start.y, end.y );
    auto width = std::max( start.x, end.x ) - x;
    auto height = std::max( start.y, end.y ) - y;

    static vector<u32> pixels;
    pixels.resize( width * height, 0 );
    Bind();
    glcall( glReadBuffer( GL_COLOR_ATTACHMENT0 ) );
    glcall( glReadPixels( x, y, width, height, GL_RED_INTEGER, GL_UNSIGNED_INT, pixels.data() ) );
    glcall( glReadBuffer( GL_NONE ) );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
    Unbind();

    return pixels;
}

Framebuffer::~Framebuffer()
{
    glcall( glDeleteFramebuffers( 1, &mRendererID ) );
}

void Framebuffer::Swap( Framebuffer& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mColorAttachment, other.mColorAttachment );
    std::swap( mDepthAttachment, other.mDepthAttachment );
    std::swap( mStencilAttachment, other.mStencilAttachment );
    std::swap( mSpecification, other.mSpecification );
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

i32 Framebuffer::Width() const
{
    return mSpecification.mWidth;
}

i32 Framebuffer::Height() const
{
    return mSpecification.mHeight;
}

void Framebuffer::Bind()
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, mRendererID ) );
    glViewport( 0, 0, Width(), Height() );
}

void Framebuffer::Unbind()
{
    glcall( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
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