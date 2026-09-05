#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/error.hpp"
#include "engine/renderer/webgpu.hpp"

namespace bubble
{
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

}
