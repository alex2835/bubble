#pragma once
#include <glm/glm.hpp>
#include "utils/error.hpp"
#include "utils/imexp.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT Texture2DSpecification
{
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint32_t mChanelFormat = GL_UNSIGNED_BYTE;   // GL_UNSIGNED_BYTE, GL_FLOAT
    uint32_t mInternalFormat = GL_RGBA8;         // GL_RED8, GL_RGB8, GL_RGBA8, GL_DEPTH_COMPONENT
    uint32_t mDataFormat = GL_RGBA;              // GL_RED, GL_RGB, GL_RGBA , GL_DEPTH_COMPONENT
    uint32_t mMinFiler = GL_LINEAR;              // GL_LINEAR, GL_NEAREST
    uint32_t mMagFilter = GL_LINEAR;             // GL_LINEAR, GL_NEAREST
    uint32_t mWrapS = GL_REPEAT;	             // GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT 
    uint32_t mWrapT = GL_REPEAT;	             // GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT 
    glm::vec4 mBorderColor = glm::vec4( 1.0f );
    bool mFlip = false;
    bool mMinMap = false;
    bool mAnisotropicFiltering = false;

    auto operator<=>( const Texture2DSpecification& ) const = default;
};
void SetTextureSpecChanels( Texture2DSpecification& spec, int channels );
uint32_t ExtractTextureSpecChannels( const Texture2DSpecification& spec );
uint32_t GetTextureSize( const Texture2DSpecification& spec );



class BUBBLE_ENGINE_EXPORT Texture2D
{
public:
    Texture2D() = default;
    // Create 1x1 texture
    //Texture2D( const glm::vec4& color );
    Texture2D( uint32_t width, uint32_t height );
    Texture2D( const Texture2DSpecification& spec );

    Texture2D( const Texture2D& ) = delete;
    Texture2D& operator= ( const Texture2D& ) = delete;

    Texture2D( Texture2D&& ) noexcept;
    Texture2D& operator= ( Texture2D&& ) noexcept;

    ~Texture2D();

    uint32_t GetRendererID() const;
    uint32_t GetWidth()  const;
    uint32_t GetHeight() const;

    // Size in bytes
    void SetData( void* data, uint32_t size );
    void GetData( void* data, uint32_t size ) const;

    void Bind( uint32_t slot = 0 ) const;
    static void UnbindAll();

    void Resize( const glm::ivec2& new_size );
    void Invalidate();

    bool operator==( const Texture2D& other ) const;

private:
    GLuint mRendererID = 0;
    Texture2DSpecification mSpecification;
};

}