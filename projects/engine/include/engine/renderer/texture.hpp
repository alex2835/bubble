#pragma once
#include "engine/utils/error.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/utils/types.hpp"

namespace bubble
{
struct BUBBLE_ENGINE_EXPORT Texture2DSpecification
{
    u32 mWidth = 0;
    u32 mHeight = 0;
    u32 mChanelFormat = GL_UNSIGNED_BYTE; // GL_UNSIGNED_BYTE, GL_FLOAT
    u32 mDataFormat = GL_RGBA;            // GL_RED, GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
    u32 mInternalFormat = GL_RGBA8;       // GL_RED8, GL_RGB8, GL_RGBA8, GL_DEPTH_COMPONENT
    u32 mMinFiler = GL_LINEAR;            // GL_LINEAR, GL_NEAREST
    u32 mMagFilter = GL_LINEAR;           // GL_LINEAR, GL_NEAREST
    u32 mWrapS = GL_REPEAT;	              // GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT 
    u32 mWrapT = GL_REPEAT;	              // GL_REPEAT, GL_CLAMP, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT 
    vec4 mBorderColor = vec4( 1.0f );
    bool mFlip = false;
    bool mMinMap = false;
    bool mAnisotropicFiltering = false;

    Texture2DSpecification( const Texture2DSpecification& ) = default;

    void SetTextureSpecChanels( i32 channels );
    u32 ExtractTextureSpecChannels() const;
    u32 GetTextureSize() const;
    static Texture2DSpecification CreateRGBA8( uvec2 size );
    static Texture2DSpecification CreateRGBA32( uvec2 size );
    static Texture2DSpecification CreateObjectId( uvec2 size );
    static Texture2DSpecification CreateDepth( uvec2 size );
private:
    Texture2DSpecification( uvec2 size );
};



class BUBBLE_ENGINE_EXPORT Texture2D
{
public:
    Texture2D( const Texture2DSpecification& spec );
    Texture2D( const Texture2D& ) = delete;
    Texture2D& operator= ( const Texture2D& ) = delete;
    Texture2D( Texture2D&& ) noexcept;
    Texture2D& operator= ( Texture2D&& ) noexcept;
    ~Texture2D();
    
    void Swap( Texture2D& other ) noexcept;

    GLuint RendererID() const;
    GLsizei Width()  const;
    GLsizei Height() const;

    // size in bytes
    void SetData( const void* data, u32 size );
    void GetData( void* data, u32 size ) const;

    void Bind( i32 slot = 0 ) const;
    static void UnbindAll();

    void Resize( const ivec2& new_size );
    void Invalidate();

    bool operator==( const Texture2D& other ) const;

private:
    GLuint mRendererID = 0;
    Texture2DSpecification mSpecification;
};

}