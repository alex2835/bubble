#include "engine/pch/pch.hpp"
#include "engine/types/array.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/log/log.hpp"

namespace bubble
{
namespace
{

// Buffer sizes and writeBuffer ranges must be multiples of 4.
u64 Align4( u64 size )
{
    return ( size + 3ull ) & ~3ull;
}

wgpu::raii::Buffer CreateBuffer( u64 size, wgpu::BufferUsage usage, string_view label )
{
    wgpu::BufferDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( label );
    desc.size = Align4( size );
    desc.usage = usage | wgpu::BufferUsage::CopyDst;
    desc.mappedAtCreation = false;
    auto buffer = wgpu::raii::Buffer( Gpu().Device().createBuffer( desc ) );
    if ( not buffer )
        LogError( "Failed to create buffer '{}' of {} bytes", label, desc.size );
    return buffer;
}

}


// The byte size of one vertex attribute of this type.
//
// Vertex attributes only. This deliberately refuses matrices and textures
// rather than answering for them: a matrix spans several attribute locations,
// and the old GLSLDataTypeSize returned 4*3*3 = 36 for a Mat3, which is right
// for a tightly packed array and wrong for the uniform address space, where a
// mat3x3 is three 16 byte columns and occupies 48. Uniform member sizes come
// from the reflected WGSL layout, never from here.
u32 VertexAttributeSize( ShaderDataType type )
{
    switch ( type )
    {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 4 * 2;
        case ShaderDataType::Float3: return 4 * 3;
        case ShaderDataType::Float4: return 4 * 4;
        case ShaderDataType::Int:    return 4;
        case ShaderDataType::Int2:   return 4 * 2;
        case ShaderDataType::Int3:   return 4 * 3;
        case ShaderDataType::Int4:   return 4 * 4;
        case ShaderDataType::UInt:   return 4;
        case ShaderDataType::Bool:   return 4;

        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4:
        case ShaderDataType::Texture2D:
            break;
    }
    BUBBLE_ASSERT( false, "Type is not usable as a vertex attribute" );
    return 0;
}

// How many scalars one value of this type holds.
//
// Scalars and vectors only. A matrix is excluded on purpose - the old function
// answered 3 for a Mat3 and 4 for a Mat4, meaning columns, which reads as a
// component count and is wrong either way (a mat3 holds nine scalars). Both
// callers in any.cpp already handle matrices in their own branches, because a
// matrix also needs per column padding that a flat count cannot express.
u32 ShaderDataComponentCount( ShaderDataType type )
{
    switch ( type )
    {
        case ShaderDataType::Float:  return 1;
        case ShaderDataType::Float2: return 2;
        case ShaderDataType::Float3: return 3;
        case ShaderDataType::Float4: return 4;
        case ShaderDataType::Int:    return 1;
        case ShaderDataType::Int2:   return 2;
        case ShaderDataType::Int3:   return 3;
        case ShaderDataType::Int4:   return 4;
        case ShaderDataType::UInt:   return 1;
        case ShaderDataType::Bool:   return 1;

        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4:
        case ShaderDataType::Texture2D:
            break;
    }
    BUBBLE_ASSERT( false, "Type has no flat component count - handle it explicitly" );
    return 0;
}

wgpu::VertexFormat ToWGPUVertexFormat( ShaderDataType type )
{
    switch ( type )
    {
        case ShaderDataType::Float:  return wgpu::VertexFormat::Float32;
        case ShaderDataType::Float2: return wgpu::VertexFormat::Float32x2;
        case ShaderDataType::Float3: return wgpu::VertexFormat::Float32x3;
        case ShaderDataType::Float4: return wgpu::VertexFormat::Float32x4;
        case ShaderDataType::Int:    return wgpu::VertexFormat::Sint32;
        case ShaderDataType::Int2:   return wgpu::VertexFormat::Sint32x2;
        case ShaderDataType::Int3:   return wgpu::VertexFormat::Sint32x3;
        case ShaderDataType::Int4:   return wgpu::VertexFormat::Sint32x4;
        case ShaderDataType::UInt:   return wgpu::VertexFormat::Uint32;
        default: break;
    }
    BUBBLE_ASSERT( false, "Type is not usable as a vertex attribute" );
    return wgpu::VertexFormat::Float32x3;
}


// ---------------------------------------------------------------------------
// VertexLayout
// ---------------------------------------------------------------------------

VertexLayout VertexLayout::FromData( const VertexBufferData& vbd )
{
    VertexLayout layout;
    layout.mVertexCount = vbd.VertexCount();

    u64 offset = 0;
    const auto add = [&]( VertexAttributeSemantic semantic, ShaderDataType type, u64 count )
    {
        // The whole point: an attribute with no data produces no slot, so no
        // pipeline ever declares one pointing outside the buffer.
        if ( count == 0 )
            return;

        const u64 size = (u64)VertexAttributeSize( type ) * count;
        layout.mSlots.push_back( VertexAttributeSlot{ semantic, type, offset, size } );
        offset += size;
    };

    add( VertexAttributeSemantic::Position,  ShaderDataType::Float3, vbd.mPositions.size() );
    add( VertexAttributeSemantic::Normal,    ShaderDataType::Float3, vbd.mNormals.size() );
    add( VertexAttributeSemantic::TexCoords, ShaderDataType::Float2, vbd.mTexCoords.size() );
    add( VertexAttributeSemantic::Tangent,   ShaderDataType::Float3, vbd.mTangents.size() );
    add( VertexAttributeSemantic::Bitangent, ShaderDataType::Float3, vbd.mBitangents.size() );

    layout.mTotalSize = offset;
    return layout;
}

bool VertexLayout::SameAttributes( const VertexLayout& other ) const
{
    if ( mSlots.size() != other.mSlots.size() )
        return false;
    for ( u64 i = 0; i < mSlots.size(); i++ )
    {
        if ( mSlots[i].mSemantic != other.mSlots[i].mSemantic or
             mSlots[i].mType != other.mSlots[i].mType )
            return false;
    }
    return true;
}

VertexLayoutDescriptors BuildVertexLayoutDescriptors( const VertexLayout& layout )
{
    VertexLayoutDescriptors out;
    const auto& slots = layout.Slots();

    // Reserved up front: mLayouts stores a pointer into mAttributes, so the
    // vector must not reallocate afterwards.
    out.mAttributes.reserve( slots.size() );
    out.mLayouts.reserve( slots.size() );

    for ( u64 i = 0; i < slots.size(); i++ )
    {
        const auto& slot = slots[i];

        wgpu::VertexAttribute attribute = wgpu::Default;
        attribute.format = ToWGPUVertexFormat( slot.mType );
        // Each attribute is bound as its own vertex buffer slot starting at its
        // block, so within that slot it sits at zero.
        attribute.offset = 0;
        attribute.shaderLocation = (u32)slot.mSemantic;
        out.mAttributes.push_back( attribute );

        wgpu::VertexBufferLayout bufferLayout = wgpu::Default;
        bufferLayout.arrayStride = VertexAttributeSize( slot.mType );
        bufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
        bufferLayout.attributeCount = 1;
        bufferLayout.attributes = &out.mAttributes[i];
        out.mLayouts.push_back( bufferLayout );
    }
    return out;
}

vector<u8> VertexBufferDataFlat( const VertexBufferData& vbd )
{
    const VertexLayout layout = VertexLayout::FromData( vbd );
    vector<u8> data( layout.TotalSize() );

    const auto copy = [&]( const VertexAttributeSlot& slot, const void* src )
    {
        std::memcpy( data.data() + slot.mByteOffset, src, slot.mByteSize );
    };

    for ( const auto& slot : layout.Slots() )
    {
        switch ( slot.mSemantic )
        {
            case VertexAttributeSemantic::Position:  copy( slot, vbd.mPositions.data() );  break;
            case VertexAttributeSemantic::Normal:    copy( slot, vbd.mNormals.data() );    break;
            case VertexAttributeSemantic::TexCoords: copy( slot, vbd.mTexCoords.data() );  break;
            case VertexAttributeSemantic::Tangent:   copy( slot, vbd.mTangents.data() );   break;
            case VertexAttributeSemantic::Bitangent: copy( slot, vbd.mBitangents.data() ); break;
        }
    }
    return data;
}


// ---------------------------------------------------------------------------
// VertexArray
// ---------------------------------------------------------------------------

VertexArray::VertexArray( VertexArray&& other ) noexcept
{
    Swap( other );
}

VertexArray& VertexArray::operator=( VertexArray&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void VertexArray::Swap( VertexArray& other ) noexcept
{
    std::swap( mVertexBuffer, other.mVertexBuffer );
    std::swap( mIndexBuffer, other.mIndexBuffer );
    std::swap( mLayout, other.mLayout );
    std::swap( mVertexBufferSize, other.mVertexBufferSize );
    std::swap( mIndexBufferSize, other.mIndexBufferSize );
    std::swap( mIndexCount, other.mIndexCount );
}

void VertexArray::SetBufferData( const VertexBufferData& vbd,
                                 const vector<u32>& indices,
                                 BufferType )
{
    mLayout = VertexLayout::FromData( vbd );
    mIndexCount = indices.size();

    if ( mLayout.TotalSize() == 0 or indices.empty() )
    {
        mVertexBuffer = {};
        mIndexBuffer = {};
        mVertexBufferSize = 0;
        mIndexBufferSize = 0;
        return;
    }

    const vector<u8> vertexData = VertexBufferDataFlat( vbd );
    const u64 indexBytes = indices.size() * sizeof( u32 );

    // Reallocate only when the data outgrows the buffer. Sizes are fixed at
    // creation, so a smaller update just writes into the existing allocation -
    // which is what the per-frame debug meshes do.
    if ( not mVertexBuffer or vertexData.size() > mVertexBufferSize )
    {
        mVertexBuffer = CreateBuffer( vertexData.size(), wgpu::BufferUsage::Vertex, "Vertex Buffer" );
        mVertexBufferSize = Align4( vertexData.size() );
    }
    if ( not mIndexBuffer or indexBytes > mIndexBufferSize )
    {
        mIndexBuffer = CreateBuffer( indexBytes, wgpu::BufferUsage::Index, "Index Buffer" );
        mIndexBufferSize = Align4( indexBytes );
    }

    Gpu().Queue().writeBuffer( *mVertexBuffer, 0, vertexData.data(), Align4( vertexData.size() ) );
    Gpu().Queue().writeBuffer( *mIndexBuffer, 0, indices.data(), Align4( indexBytes ) );
}

void VertexArray::Bind( wgpu::RenderPassEncoder pass ) const
{
    if ( not Valid() )
        return;

    const auto& slots = mLayout.Slots();
    for ( u32 i = 0; i < (u32)slots.size(); i++ )
        pass.setVertexBuffer( i, *mVertexBuffer, slots[i].mByteOffset, slots[i].mByteSize );

    pass.setIndexBuffer( *mIndexBuffer, wgpu::IndexFormat::Uint32, 0, mIndexCount * sizeof( u32 ) );
}

}
