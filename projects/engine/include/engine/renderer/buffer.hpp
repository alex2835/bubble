#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/error.hpp"
#include "engine/renderer/webgpu.hpp"

namespace bubble
{
class UniformBuffer;
struct UniformArrayElement;

enum class GLSLDataType
{
    Texture2D,
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
    Bool,
    // WGSL distinguishes i32 from u32, and the entity id shader needs one.
    UInt,
};
u32 GLSLDataTypeSize( GLSLDataType type );
u32 GLSLDataComponentCount( GLSLDataType type );

// Uniform address space layout. WGSL's rules match std140 for everything used
// here, so the existing offsets carry over unchanged.
u32 Std140DataTypeSize( GLSLDataType type );
u32 Std140DataTypeAligment( GLSLDataType type );

wgpu::VertexFormat ToWGPUVertexFormat( GLSLDataType type );


// Advisory only. WebGPU has no equivalent of GL_STATIC_DRAW / GL_DYNAMIC_DRAW -
// every buffer is created with CopyDst and written through the queue - but the
// distinction still says something about intent at the call site.
enum class BufferType
{
    Static,
    Dynamic
};


// ---------------------------------------------------------------------------
// Vertex data
// ---------------------------------------------------------------------------

// Vertex attribute locations, matching the WGSL @location on the vertex stage.
enum class VertexAttributeSemantic : u32
{
    Position  = 0,
    Normal    = 1,
    TexCoords = 2,
    Tangent   = 3,
    Bitangent = 4,
};

struct VertexBufferData
{
    vector<vec3> mPositions;
    vector<vec3> mNormals;
    vector<vec2> mTexCoords;
    vector<vec3> mTangents;
    vector<vec3> mBitangents;

    u64 VertexCount() const { return mPositions.size(); }

    void Clear()
    {
        mPositions.clear();
        mNormals.clear();
        mTexCoords.clear();
        mTangents.clear();
        mBitangents.clear();
    }
};


// Where one attribute's block lives inside the packed vertex buffer.
//
// Vertex data is blocked, not interleaved: every position, then every normal,
// and so on. WebGPU handles that by binding the same buffer to several vertex
// slots at different offsets, one slot per attribute.
struct VertexAttributeSlot
{
    VertexAttributeSemantic mSemantic;
    GLSLDataType mType;
    u64 mByteOffset = 0;
    u64 mByteSize = 0;

    bool operator==( const VertexAttributeSlot& ) const = default;
};


// The set of attributes a mesh actually has.
//
// Only non-empty arrays produce a slot. The layout this replaced emitted all
// five unconditionally, with mCount taken straight from the (possibly empty)
// array, and then enabled a vertex attribute for each - so a mesh without
// tangents got attributes pointing past the end of its own buffer. OpenGL
// tolerated that as long as the shader did not read those locations, which is
// why the billboard quad worked; WebGPU rejects it at pipeline creation.
class VertexLayout
{
public:
    VertexLayout() = default;
    static VertexLayout FromData( const VertexBufferData& vbd );

    const vector<VertexAttributeSlot>& Slots() const { return mSlots; }
    u64 TotalSize() const { return mTotalSize; }
    u64 VertexCount() const { return mVertexCount; }
    bool Empty() const { return mSlots.empty(); }

    // Two layouts are interchangeable for pipeline purposes when they have the
    // same attributes; vertex count is deliberately not part of that.
    bool SameAttributes( const VertexLayout& other ) const;

    bool operator==( const VertexLayout& ) const = default;

private:
    vector<VertexAttributeSlot> mSlots;
    u64 mTotalSize = 0;
    u64 mVertexCount = 0;
};


// Owns the wgpu descriptor arrays a pipeline points at. They must outlive the
// createRenderPipeline call, so the caller keeps this alive across it.
struct VertexLayoutDescriptors
{
    vector<wgpu::VertexAttribute> mAttributes;
    vector<wgpu::VertexBufferLayout> mLayouts;
};
VertexLayoutDescriptors BuildVertexLayoutDescriptors( const VertexLayout& layout );

// Flattens the five arrays into one blocked buffer, matching VertexLayout.
vector<u8> VertexBufferDataFlat( const VertexBufferData& vbd );


// A mesh's GPU buffers.
//
// Still called VertexArray, but there is no vertex array object any more - the
// attribute layout belongs to the pipeline, and this just owns the vertex and
// index buffers and knows how to bind them.
class VertexArray
{
public:
    VertexArray() = default;
    VertexArray( const VertexArray& ) = delete;
    VertexArray& operator=( const VertexArray& ) = delete;
    VertexArray( VertexArray&& ) noexcept;
    VertexArray& operator=( VertexArray&& ) noexcept;
    ~VertexArray() = default;

    void Swap( VertexArray& other ) noexcept;

    void SetBufferData( const VertexBufferData& vbd,
                        const vector<u32>& indices,
                        BufferType type = BufferType::Static );

    // setVertexBuffer for every slot, then setIndexBuffer.
    void Bind( wgpu::RenderPassEncoder pass ) const;

    const VertexLayout& Layout() const { return mLayout; }
    u64 IndexCount() const { return mIndexCount; }
    bool Valid() const { return (bool)mVertexBuffer and mIndexCount > 0; }

private:
    wgpu::raii::Buffer mVertexBuffer;
    wgpu::raii::Buffer mIndexBuffer;
    VertexLayout mLayout;
    u64 mVertexBufferSize = 0;
    u64 mIndexBufferSize = 0;
    u64 mIndexCount = 0;
};


// ---------------------------------------------------------------------------
// Uniform buffers
// ---------------------------------------------------------------------------

struct BufferElement
{
    std::string_view mName;
    GLSLDataType mType;
    u64 mCount = 1;
    bool mNormalized = false;
    u64 mSize = 0;
    u64 mOffset = 0;

    bool operator ==( const BufferElement& ) const = default;
};

// Named, std140-laid-out fields of a uniform buffer. This used to describe
// vertex attributes too; that job moved to VertexLayout, which does not need
// names and does need to skip absent attributes.
class VertexBufferLayout
{
public:
    VertexBufferLayout() = default;
    VertexBufferLayout( const std::initializer_list<BufferElement>& elements );

    void SetStride( u64 stride );
    u64 Stride() const;
    u64 Size() const;
    const vector<BufferElement>& Elements() const;

    bool operator == ( const VertexBufferLayout& ) const = default;
    vector<BufferElement>::iterator begin();
    vector<BufferElement>::iterator end();
    vector<BufferElement>::const_iterator begin() const;
    vector<BufferElement>::const_iterator end() const;

private:
    void CalculateOffsetsAndStride();
    vector<BufferElement> mElements;
    u64 mStride = 0;
};


// A uniform buffer with a CPU mirror.
//
// Writes land in the staging blob and reach the GPU in one queue.writeBuffer at
// Flush. The version this replaced issued a bind, a sub-buffer upload and an
// unbind for every scalar field, so filling the light array cost roughly 1280
// upload calls per frame to move ten kilobytes.
class UniformBuffer
{
public:
    UniformBuffer( i32 index, string name, const VertexBufferLayout& layout, u32 size = 1 );
    UniformBuffer( const UniformBuffer& ) = delete;
    UniformBuffer& operator=( const UniformBuffer& ) = delete;
    UniformBuffer( UniformBuffer&& ) noexcept;
    UniformBuffer& operator=( UniformBuffer&& ) noexcept;
    ~UniformBuffer() = default;

    void Swap( UniformBuffer& other ) noexcept;

    // Raw write into the staging blob (std140 padding is the caller's problem).
    void SetData( const void* data, u32 size, u32 offset = 0 );

    // Uploads the staging blob. Pass a count to send only the first N elements,
    // which is what the light buffer wants - there is no point moving 128
    // lights' worth of bytes when the scene has three.
    void Flush( u64 elementCount = 0 );

    wgpu::Buffer GetBuffer() const { return *mBuffer; }
    const string& Name() const;
    u64 Index() const;

    UniformArrayElement operator[] ( u64 index );
    UniformArrayElement Element ( u64 index );

    const VertexBufferLayout& Layout() const;
    u64 BufferSize() const;
    u64 Size() const;

    u8* StagingAt( u64 offset );

private:
    void CalculateOffsetsAndStride();

    wgpu::raii::Buffer mBuffer;
    vector<u8> mStaging;
    string mName;
    VertexBufferLayout mLayout;
    u64 mBufferSize = 0;
    u64 mSize = 0; // elements in the array
    u64 mIndex = 0;
};


// Points at one element of a UniformBuffer's staging blob. Owns nothing, and
// nothing it writes reaches the GPU until UniformBuffer::Flush.
struct UniformArrayElement
{
    UniformBuffer* mBuffer = nullptr;
    u64 mArrayIndex = 0;

    UniformArrayElement( UniformBuffer& uniformBuffer, u64 index );

    void SetData( const void* data, u64 size = 0, u64 offset = 0 );

    void SetInt( const string& name, i32 data );
    void SetUInt( const string& name, u32 data );
    void SetFloat( const string& name, f32 data );
    void SetFloat2( const string& name, const vec2& data );
    void SetFloat3( const string& name, const vec3& data );
    void SetFloat4( const string& name, const vec4& data );
    void SetMat3( const string& name, const mat3& data );
    void SetMat4( const string& name, const mat4& data );

private:
    const BufferElement& FindBufferElement( const string& name, GLSLDataType type );
    void SetRawData( const BufferElement& elem, const void* data, u64 size );
};

}
