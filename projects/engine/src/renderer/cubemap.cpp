
#include <stb_image.h>
#include "engine/renderer/cubemap.hpp"

namespace bubble
{
Cubemap::Cubemap( i32 width, i32 height, const Texture2DSpecification& spec )
{
    glcall( glGenTextures( 1, &mRendererID ) );
    glcall( glBindTexture( GL_TEXTURE_CUBE_MAP, mRendererID ) );

    for( i32 i = 0; i < 6; ++i )
    {
        glcall( glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, spec.mInternalFormat,
                              width, height, 0, spec.mDataFormat, GL_FLOAT, nullptr ) );
    }

    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, spec.mMagFilter ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, spec.mMinFiler ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE ) );
}

Cubemap::Cubemap( Cubemap&& other ) noexcept
{
    mRendererID = other.mRendererID;
    other.mRendererID = 0;
}

Cubemap& Cubemap::operator=( Cubemap&& other ) noexcept
{
    if( this != &other )
    {
        glcall( glDeleteTextures( 1, &mRendererID ) );
        mRendererID = other.mRendererID;
        other.mRendererID = 0;
    }
    return *this;
}

Cubemap::Cubemap( const std::array<Scope<u8[]>, 6>& data, 
                  const Texture2DSpecification& spec )
{
    glcall( glGenTextures( 1, &mRendererID ) );
    glcall( glBindTexture( GL_TEXTURE_CUBE_MAP, mRendererID ) );

    for( GLuint i = 0; i < 6; i++ )
    {
        glcall( glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, spec.mInternalFormat,
                              spec.mWidth, spec.mHeight, 0, spec.mDataFormat, GL_UNSIGNED_BYTE, data[i].get() ) );
    }

    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, spec.mMagFilter ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, spec.mMinFiler ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );
    glcall( glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE ) );
}

Cubemap::~Cubemap()
{
    glcall( glDeleteTextures( 1, &mRendererID ) );
}

void Cubemap::Bind( i32 slot )
{
    glcall( glActiveTexture( GL_TEXTURE0 + slot ) );
    glcall( glBindTexture( GL_TEXTURE_CUBE_MAP, mRendererID ) );
}

}