#pragma once
#include <vector>
#include <string>
#include <cassert>
#include "utils/imexp.hpp"
#include "utils/error.hpp"

namespace bubble
{

enum class GLSLDataType
{
    None,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Bool
};
uint32_t GLSLDataTypeSize( GLSLDataType type );
uint32_t GLSLDataComponentCount( GLSLDataType type );


struct BUBBLE_ENGINE_EXPORT BufferElement
{
    std::string  mName;
    GLSLDataType mType;
    size_t	     mSize;
    size_t	     mCount;
    size_t		 mOffset;
    bool		 mNormalized;
    
    BufferElement( GLSLDataType type,
                   const std::string& name,
                   size_t count = 1,
                   bool normalized = false );
};


struct BUBBLE_ENGINE_EXPORT BufferLayout
{
    std::vector<BufferElement> mElements;
    size_t mStride = 0;

    BufferLayout() = default;
    BufferLayout( const std::initializer_list<BufferElement>& elements );

    size_t GetStride() const;
    const std::vector<BufferElement>& GetElements() const;

    std::vector<BufferElement>::iterator begin();
    std::vector<BufferElement>::iterator end();
    std::vector<BufferElement>::const_iterator begin() const;
    std::vector<BufferElement>::const_iterator end() const;

private:
    void CalculateOffsetsAndStride();
};


struct VertexBuffer
{
    GLuint mRendererID = 0;
    size_t mSize = 0;
    BufferLayout mLayout;

    VertexBuffer() = default;
    VertexBuffer( size_t size );
    VertexBuffer( void* vertices, size_t size );

    VertexBuffer( const VertexBuffer& ) = delete;
    VertexBuffer& operator=( const VertexBuffer& ) = delete;

    VertexBuffer( VertexBuffer&& ) noexcept;
    VertexBuffer& operator=( VertexBuffer&& ) noexcept;

    ~VertexBuffer();

    void Bind() const;
    void Unbind() const;

    void SetData( const void* data, uint32_t size );

    const BufferLayout& GetLayout() const;
    void SetLayout( const BufferLayout& layout );

    size_t GetSize();
};


struct BUBBLE_ENGINE_EXPORT IndexBuffer
{
    GLuint mRendererID = 0;
    size_t mCount = 0;

    IndexBuffer() = default;
    IndexBuffer( uint32_t* indices, size_t count );

    IndexBuffer( const IndexBuffer& ) = delete;
    IndexBuffer& operator=( const IndexBuffer& ) = delete;

    IndexBuffer( IndexBuffer&& ) noexcept;
    IndexBuffer& operator=( IndexBuffer&& ) noexcept;

    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;

    size_t GetCount() const;
};

}