#include "engine/pch/pch.hpp"
#include <GL/glew.h>
#include <stb_image.h>
#include "engine/renderer/texture.hpp"

namespace bubble
{

void Texture2DSpecification::SetTextureSpecChanels( i32 channels )
{
    switch( channels )
    {
    case 1:
        mInternalFormat = GL_R8;
        mDataFormat = GL_RED;
        break;
    case 3:
        mInternalFormat = GL_RGB8;
        mDataFormat = GL_RGB;
        break;
    case 4:
        mInternalFormat = GL_RGBA8;
        mDataFormat = GL_RGBA;
        break;
    default:
        BUBBLE_ASSERT( false, "Format not supported!" );
    }
}

u32 Texture2DSpecification::ExtractTextureSpecChannels() const
{
    u32 bpp = 0;
    switch( mDataFormat )
    {
    case GL_RGBA:
        bpp = 4;
        break;
    case GL_RGB:
        bpp = 3;
        break;
    case GL_RED:
        bpp = 1;
        break;
    default:
        BUBBLE_ASSERT( false, "Format not supported!" );
    }
    return bpp;
}

u32 Texture2DSpecification::GetTextureSize() const
{
    return mWidth * mHeight * ExtractTextureSpecChannels();
}


Texture2DSpecification Texture2DSpecification::CreateRGBA8( uvec2 size )
{
    return Texture2DSpecification( size );
}

bubble::Texture2DSpecification Texture2DSpecification::CreateRGBA32( uvec2 size )
{
    Texture2DSpecification specification( size );
    specification.mChanelFormat = GL_UNSIGNED_INT;
    specification.mDataFormat = GL_RGBA_INTEGER;
    specification.mInternalFormat = GL_RGBA32I;
    return specification;
}

Texture2DSpecification Texture2DSpecification::CreateObjectId( uvec2 size )
{
    Texture2DSpecification specification( size );
    specification.mChanelFormat = GL_UNSIGNED_INT;
    specification.mDataFormat = GL_RED_INTEGER;
    specification.mInternalFormat = GL_R32UI;
    specification.mMinFiler = GL_NEAREST;
    specification.mMagFilter = GL_NEAREST;
    specification.mWrapS = GL_CLAMP_TO_BORDER;
    specification.mWrapT = GL_CLAMP_TO_BORDER;
    return specification;
}

Texture2DSpecification Texture2DSpecification::CreateDepth( uvec2 size )
{
    Texture2DSpecification specification( size );
    specification.mChanelFormat = GL_FLOAT;
    specification.mDataFormat = GL_DEPTH_COMPONENT;
    specification.mInternalFormat = GL_DEPTH_COMPONENT32F;
    specification.mWrapS = GL_CLAMP_TO_BORDER;
    specification.mWrapT = GL_CLAMP_TO_BORDER;
    return specification;
}

Texture2DSpecification::Texture2DSpecification( uvec2 size )
    : mWidth( size.x ),
      mHeight( size.y ),
      mChanelFormat( GL_UNSIGNED_BYTE ),
      mDataFormat( GL_RGBA ),
      mInternalFormat( GL_RGBA8 ),
      mMinFiler( GL_LINEAR ),
      mMagFilter( GL_LINEAR ),
      mWrapS( GL_REPEAT ),
      mWrapT( GL_REPEAT ),
      mBorderColor( vec4( 1.0f ) ),
      mFlip( false ),
      mMinMap( false ),
      mAnisotropicFiltering( false )
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

Texture2D::~Texture2D()
{
    glDeleteTextures( 1, &mRendererID );
}

void Texture2D::Swap( Texture2D& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mSpecification, other.mSpecification );
}

void Texture2D::SetData( const void* data, u32 size )
{
    Bind();
    [[maybe_unused]]u32 channels = mSpecification.ExtractTextureSpecChannels();
    BUBBLE_ASSERT( size == mSpecification.mWidth * mSpecification.mHeight * channels, "Data must be entire texture!" );
    glcall( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
            mSpecification.mWidth, mSpecification.mHeight, mSpecification.mDataFormat, mSpecification.mChanelFormat, data ) );
}

void Texture2D::GetData( void* data, u32 size ) const
{
    Bind();
    [[maybe_unused]] u32 channels = mSpecification.ExtractTextureSpecChannels();
    BUBBLE_ASSERT( size == mSpecification.mWidth * mSpecification.mHeight * channels, "Data must be entire texture!" );
    glcall( glGetTexImage( GL_TEXTURE_2D, 0, mSpecification.mDataFormat, mSpecification.mChanelFormat, data ) );
}

void Texture2D::Bind( i32 slot ) const
{
    glActiveTexture( GL_TEXTURE0 + slot );
    glBindTexture( GL_TEXTURE_2D, mRendererID );
}

void Texture2D::UnbindAll()
{
    glActiveTexture( GL_TEXTURE0 );
}

void Texture2D::Resize( const ivec2& new_size )
{
    mSpecification.mWidth = new_size.x;
    mSpecification.mHeight = new_size.y;
    Invalidate();
}

i32 Texture2D::Width() const
{
    return mSpecification.mWidth;
}

i32 Texture2D::Height() const
{
    return mSpecification.mHeight;
}

u32 Texture2D::RendererID() const
{
    return mRendererID;
}

void Texture2D::Invalidate()
{
    glcall( glDeleteTextures( 1, &mRendererID ) );

    glcall( glGenTextures( 1, &mRendererID ) );
    glcall( glBindTexture( GL_TEXTURE_2D, mRendererID ) );
    glcall( glTexImage2D( GL_TEXTURE_2D, 0, mSpecification.mInternalFormat,
            mSpecification.mWidth, mSpecification.mHeight, 0, mSpecification.mDataFormat, mSpecification.mChanelFormat, nullptr ) );

    glcall( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mSpecification.mMinFiler ) );
    glcall( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mSpecification.mMagFilter ) );

    glcall( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mSpecification.mWrapS ) );
    glcall( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mSpecification.mWrapS ) );

    if( mSpecification.mWrapS == GL_CLAMP_TO_BORDER || 
        mSpecification.mWrapT == GL_CLAMP_TO_BORDER )
    {
        glcall( glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (f32*)&mSpecification.mBorderColor ) );
    }

    if( mSpecification.mAnisotropicFiltering )
    {
        GLfloat value;
        glcall( glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &value ) );
        glcall( glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, value ) );
    }

    if( mSpecification.mMinMap && 
        !mSpecification.mAnisotropicFiltering )
    {
        glcall( glGenerateMipmap( GL_TEXTURE_2D ) );
    }
}

bool Texture2D::operator==( const Texture2D& other ) const
{
    return mRendererID == other.mRendererID;
}

}