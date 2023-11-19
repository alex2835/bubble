
#include <stb_image.h>
#include "renderer/texture.hpp"

namespace bubble
{

void SetTextureSpecChanels( Texture2DSpecification& spec, int channels )
{
    switch( channels )
    {
    case 1:
        spec.mInternalFormat = GL_R8;
        spec.mDataFormat = GL_RED;
        break;
    case 3:
        spec.mInternalFormat = GL_RGB8;
        spec.mDataFormat = GL_RGB;
        break;
    case 4:
        spec.mInternalFormat = GL_RGBA8;
        spec.mDataFormat = GL_RGBA;
        break;
    default:
        BUBBLE_ASSERT( false, "Format not supported!" );
    }
}

uint32_t ExtractTextureSpecChannels( const Texture2DSpecification& spec )
{
    uint32_t bpp = 0;
    switch( spec.mDataFormat )
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

uint32_t GetTextureSize( const Texture2DSpecification& spec )
{
    return spec.mWidth * spec.mHeight * ExtractTextureSpecChannels( spec );
}



//Texture2D::Texture2D( const glm::vec4& color )
//{
//    mSpecification.mWidth = 1;
//    mSpecification.mHeight = 1;
//    mSpecification.mInternalFormat = GL_RGBA8;
//    mSpecification.mDataFormat = GL_RGBA;
//    mSpecification.mChanelFormat = GL_FLOAT;
//    mSpecification.mMinFiler = GL_NEAREST;
//    mSpecification.mMagFilter = GL_NEAREST;
//    mSpecification.mWrapS = GL_REPEAT;
//    mSpecification.mWrapT = GL_REPEAT;
//    Invalidate();
//    SetData( (void*)&color, 4 );
//}

Texture2D::Texture2D( const Texture2DSpecification& spec )
    : mSpecification( spec )
{
    Invalidate();
}

Texture2D::Texture2D( uint32_t width, uint32_t height )
    : mSpecification( { width, height } )
{
    Invalidate();
}

Texture2D::Texture2D( Texture2D&& other ) noexcept
    : mRendererID( other.mRendererID ),
    mSpecification( other.mSpecification )
{
    other.mSpecification.mWidth = 0;
    other.mSpecification.mHeight = 0;
    other.mRendererID = 0;
}

Texture2D& Texture2D::operator=( Texture2D&& other ) noexcept
{
    if( this != &other )
    {
        glDeleteTextures( 1, &mRendererID );
        mRendererID = other.mRendererID;
        mSpecification = other.mSpecification;
        other.mRendererID = 0;
        other.mSpecification.mWidth = 0;
        other.mSpecification.mHeight = 0;
    }
    return *this;
}

Texture2D::~Texture2D()
{
    glDeleteTextures( 1, &mRendererID );
}

void Texture2D::SetData( void* data, uint32_t size )
{
    uint32_t channels = ExtractTextureSpecChannels( mSpecification );
    BUBBLE_ASSERT( size == mSpecification.mWidth * mSpecification.mHeight * channels, "Data must be entire texture!" );
    glcall( glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
            mSpecification.mWidth, mSpecification.mHeight, mSpecification.mDataFormat, mSpecification.mChanelFormat, data ) );
}

void Texture2D::GetData( void* data, uint32_t size ) const
{
    Bind();
    uint32_t channels = ExtractTextureSpecChannels( mSpecification );
    BUBBLE_ASSERT( size == mSpecification.mWidth * mSpecification.mHeight * channels, "Data must be entire texture!" );
    glcall( glGetTexImage( GL_TEXTURE_2D, 0, mSpecification.mDataFormat, mSpecification.mChanelFormat, data ) );
}

void Texture2D::Bind( uint32_t slot ) const
{
    glActiveTexture( GL_TEXTURE0 + slot );
    glBindTexture( GL_TEXTURE_2D, mRendererID );
}

void Texture2D::UnbindAll()
{
    glActiveTexture( GL_TEXTURE0 );
}

void Texture2D::Resize( const glm::ivec2& new_size )
{
    mSpecification.mWidth = new_size.x;
    mSpecification.mHeight = new_size.y;
    Invalidate();
}

uint32_t Texture2D::GetWidth()  const
{
    return mSpecification.mWidth;
}

uint32_t Texture2D::GetHeight() const
{
    return mSpecification.mHeight;
}

uint32_t Texture2D::GetRendererID() const
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
        glcall( glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (float*)&mSpecification.mBorderColor ) );
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