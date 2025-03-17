#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/utils/error.hpp"

namespace bubble
{
class BufferLayout;
class VertexBuffer;
struct UniformArrayElement;
class UniformArray;

enum class GLSLDataType
{
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
u32 Std140DataTypeAligment( GLSLDataType type );
i32 GLSLDataTypeToOpenGLBasemType( GLSLDataType mType );


struct BufferElement
{
    std::string_view mName;
    GLSLDataType mType;
    u64 mCount = 1;
    bool mNormalized = false;
    u64 mSize = 0;
    u64 mOffset = 0;
};


class BufferLayout
{
public:
    BufferLayout() = default;
    BufferLayout( const std::initializer_list<BufferElement>& elements );
    ~BufferLayout();

    void SetStride( u64 stride );
    u64 Stride() const;
    u64 Size() const;
    const vector<BufferElement>& Elements() const;

    vector<BufferElement>::iterator begin();
    vector<BufferElement>::iterator end();
    vector<BufferElement>::const_iterator begin() const;
    vector<BufferElement>::const_iterator end() const;

private:
    void CalculateOffsetsAndStride();
    vector<BufferElement> mElements;
    u64 mStride = 0;
};


class VertexBuffer
{
public:
    VertexBuffer( const BufferLayout& layout, u64 size );
    VertexBuffer( const BufferLayout& layout, void* vertices, u64 size );
    VertexBuffer( const VertexBuffer& ) = delete;
    VertexBuffer& operator=( const VertexBuffer& ) = delete;
    VertexBuffer( VertexBuffer&& ) noexcept;
    VertexBuffer& operator=( VertexBuffer&& ) noexcept;
    ~VertexBuffer();

    void Swap( VertexBuffer& other ) noexcept;

    void Bind() const;
    void Unbind() const;
    void SetData( const void* data, u32 size );
    void SetLayout( const BufferLayout& layout );
    const BufferLayout& Layout() const;
    u64 Size();

//private:
    u32 mRendererID = 0;
    BufferLayout mLayout;
    u64 mSize = 0;
};


class IndexBuffer
{
public:
    IndexBuffer() = default;
    IndexBuffer( u32* indices, u64 count );
    IndexBuffer( const IndexBuffer& ) = delete;
    IndexBuffer& operator=( const IndexBuffer& ) = delete;
    IndexBuffer( IndexBuffer&& ) noexcept;
    IndexBuffer& operator=( IndexBuffer&& ) noexcept;
    ~IndexBuffer();

    void Swap( IndexBuffer& other ) noexcept;

    void Bind() const;
    void Unbind() const;
    u64 GetCount() const;

//private:
    u32 mRendererID = 0;
    u64 mCount = 0;
};


class VertexArray
{
public:
    VertexArray() noexcept;
    VertexArray( const VertexArray& ) = delete;
    VertexArray& operator=( const VertexArray& ) = delete;
    VertexArray( VertexArray&& ) noexcept;
    VertexArray& operator=( VertexArray&& ) noexcept;
    ~VertexArray();

    void Swap( VertexArray& other ) noexcept;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer( VertexBuffer&& vertexBuffer );
    void SetIndexBuffer( IndexBuffer&& indexBuffer );

    u32 RendererID() const;
    vector<VertexBuffer>& GetVertexBuffers();
    IndexBuffer& GetIndexBuffer();
    void VertexBufferIndex( u32 val );

private:
    u32 mRendererID = 0;
    u32 mVertexBufferIndex = 0;
    vector<VertexBuffer> mVertexBuffers;
    IndexBuffer mIndexBuffer;
};



class UniformBuffer
{
public:
    // additional size necessary if buffer contain more then one array (for example nLights)
    UniformBuffer( i32 index, string name, const BufferLayout& layout, u32 size = 1);
    UniformBuffer( const UniformBuffer& ) = delete;
    UniformBuffer& operator=( const UniformBuffer& ) = delete;
    UniformBuffer( UniformBuffer&& ) noexcept;
    UniformBuffer& operator=( UniformBuffer&& ) noexcept;
    ~UniformBuffer();

    void Swap( UniformBuffer& other ) noexcept;

    // Raw (Don't forget to fallow std140 paddings)
    void SetData( const void* data, u32 size, u32 offset = 0 );

    i32 RendererID() const;
    const string& Name() const;
    u64 Index() const;

    UniformArrayElement operator[] ( u64 index );
    UniformArrayElement Element ( u64 index );

    const BufferLayout& Layout() const;
    // Return size in bytes
    u64 BufferSize();
    // Return size of elements
    u64 Size();

private:
    // Recalculate size and offset of elements for std140
    void CalculateOffsetsAndStride();

    u32 mRendererID = 0;
    string mName;
    BufferLayout mLayout;
    u64 mBufferSize = 0;
    u64 mSize = 0; // elements in array
    u64 mIndex = 0;
};


/*
    Doesn't own any resources. Point to current element in array
*/
struct UniformArrayElement
{
    u32 mRendererID = 0;
    u64 mArrayIndex = 0;
    const BufferLayout& mLayout;

    UniformArrayElement( const UniformBuffer& uniform_buffer, u64 index );

    // Raw
    void SetData( const void* data, u64 size = 0, u64 offset = 0 );

    // Save
    void SetInt( const string& name, i32 data );
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