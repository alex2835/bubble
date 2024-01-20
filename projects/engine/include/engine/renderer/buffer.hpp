#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <cassert>
#include "engine/utils/imexp.hpp"
#include "engine/utils/error.hpp"
#include "engine/utils/types.hpp"

namespace bubble
{
class BufferLayout;
class VertexBuffer;
struct UniformArrayElemnt;
class UniformArray;

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
u32 GLSLDataTypeSize( GLSLDataType type );
u32 GLSLDataComponentCount( GLSLDataType type );
u32 Std140DataTypeSize( GLSLDataType type );
u32 Std140DataTypePadding( GLSLDataType type );
GLenum GLSLDataTypeToOpenGLBasemType( GLSLDataType mType );


struct BufferElement
{
    std::string_view mName;
    GLSLDataType mType;
    size_t mCount = 1;
    bool mNormalized = false;
    size_t mSize = 0;
    size_t mOffset = 0;
};


class BUBBLE_ENGINE_EXPORT BufferLayout
{
public:
    BufferLayout() = default;
    BufferLayout( const std::initializer_list<BufferElement>& elements );
    ~BufferLayout();

    size_t GetStride() const;
    size_t Size() const;
    const vector<BufferElement>& GetElements() const;

    vector<BufferElement>::iterator begin();
    vector<BufferElement>::iterator end();
    vector<BufferElement>::const_iterator begin() const;
    vector<BufferElement>::const_iterator end() const;

//private:
    void CalculateOffsetsAndStride();
    vector<BufferElement> mElements;
    size_t mStride = 0;
};


class BUBBLE_ENGINE_EXPORT VertexBuffer
{
public:
    VertexBuffer( const BufferLayout& layout, size_t size );
    VertexBuffer( const BufferLayout& layout, void* vertices, size_t size );

    VertexBuffer( const VertexBuffer& ) = delete;
    VertexBuffer& operator=( const VertexBuffer& ) = delete;

    VertexBuffer( VertexBuffer&& ) noexcept;
    VertexBuffer& operator=( VertexBuffer&& ) noexcept;

    ~VertexBuffer();

    void Bind() const;
    void Unbind() const;
    void SetData( const void* data, u32 size );
    void SetLayout( const BufferLayout& layout );
    const BufferLayout& GetLayout() const;
    size_t GetSize();

//private:
    GLuint mRendererID = 0;
    size_t mSize = 0;
    BufferLayout mLayout;
};


class BUBBLE_ENGINE_EXPORT IndexBuffer
{
public:
    IndexBuffer() = default;
    IndexBuffer( u32* indices, size_t count );

    IndexBuffer( const IndexBuffer& ) = delete;
    IndexBuffer& operator=( const IndexBuffer& ) = delete;

    IndexBuffer( IndexBuffer&& ) noexcept;
    IndexBuffer& operator=( IndexBuffer&& ) noexcept;

    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;
    size_t GetCount() const;

//private:
    GLuint mRendererID = 0;
    size_t mCount = 0;
};


class BUBBLE_ENGINE_EXPORT VertexArray
{
public:
    VertexArray() noexcept;
    ~VertexArray();

    VertexArray( const VertexArray& ) = delete;
    VertexArray& operator=( const VertexArray& ) = delete;

    VertexArray( VertexArray&& ) noexcept;
    VertexArray& operator=( VertexArray&& ) noexcept;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer( VertexBuffer&& vertexBuffer );
    void SetIndexBuffer( IndexBuffer&& indexBuffer );

    u32 GetRendererID() const;
    vector<VertexBuffer>& GetVertexBuffers();
    IndexBuffer& GetIndexBuffer();
    void VertexBufferIndex( u32 val );

private:
    GLuint mRendererID = 0;
    GLuint mVertexBufferIndex = 0;
    vector<VertexBuffer> mVertexBuffers;
    IndexBuffer mIndexBuffer;
};



class BUBBLE_ENGINE_EXPORT UniformBuffer
{
public:
    UniformBuffer() = default;
    // additional size necessary if buffer contain more then one array (for example nLights)
    UniformBuffer( i32 index, const BufferLayout& layout, u32 size = 1, u32 additional_size = 0 );

    UniformBuffer( const UniformBuffer& ) = delete;
    UniformBuffer& operator=( const UniformBuffer& ) = delete;

    UniformBuffer( UniformBuffer&& ) noexcept;
    UniformBuffer& operator=( UniformBuffer&& ) noexcept;

    ~UniformBuffer();

    // Raw (Don't forget to observe std140 paddings)
    void SetData( const void* data, u32 size, u32 offset = 0 );

    // Save (Use it event only one element in buffer)
    UniformArrayElemnt operator[] ( i32 index );

    const BufferLayout& GetLayout() const;
    // Return size in bytes
    size_t GetBufferSize();
    // Return size of elements
    size_t GetSize();

//private:
    // Recalculate size and offset of elements for std140
    void CalculateOffsetsAndStride();

    GLuint mRendererID = 0;
    size_t mBufferSize = 0;
    BufferLayout mLayout;
    size_t mSize = 0; // elements in array
    size_t mIndex = 0;
};


/*
    Doesn't own any resources
    Point to current element in array
*/
struct BUBBLE_ENGINE_EXPORT UniformArrayElemnt
{
    u32 mRendererID = 0;
    u32 mArrayIndex = 0;
    const BufferLayout* mLayout;

    UniformArrayElemnt( const UniformBuffer& uniform_buffer, i32 index );

    // Raw
    void SetData( const void* data, size_t size = 0, u32 offset = 0 );

    // Save
    void SetInt( const string& name, i32   data );
    void SetFloat( const string& name, f32 data );
    void SetFloat2( const string& name, const vec2& data );
    void SetFloat3( const string& name, const vec3& data );
    void SetFloat4( const string& name, const vec4& data );
    void SetMat4( const string& name, const mat4& data );

private:
    const BufferElement& FindBufferElement( const string& name, GLSLDataType type );
    void SetRawData( const BufferElement& elem, const void* data );
};

}