#include "engine/pch/pch.hpp"
#include "glm/gtc/type_ptr.hpp"
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


u32 GLSLDataTypeSize( GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Float:  return 4;
        case GLSLDataType::Float2: return 4 * 2;
        case GLSLDataType::Float3: return 4 * 3;
        case GLSLDataType::Float4: return 4 * 4;
        case GLSLDataType::Mat3:   return 4 * 3 * 3;
        case GLSLDataType::Mat4:   return 4 * 4 * 4;
        case GLSLDataType::Int:    return 4;
        case GLSLDataType::Int2:   return 4 * 2;
        case GLSLDataType::Int3:   return 4 * 3;
        case GLSLDataType::Int4:   return 4 * 4;
        case GLSLDataType::UInt:   return 4;
        case GLSLDataType::Bool:   return 4;
        case GLSLDataType::Texture2D: return 0;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}

u32 GLSLDataComponentCount( GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Float:  return 1;
        case GLSLDataType::Float2: return 2;
        case GLSLDataType::Float3: return 3;
        case GLSLDataType::Float4: return 4;
        case GLSLDataType::Mat3:   return 3;
        case GLSLDataType::Mat4:   return 4;
        case GLSLDataType::Int:    return 1;
        case GLSLDataType::Int2:   return 2;
        case GLSLDataType::Int3:   return 3;
        case GLSLDataType::Int4:   return 4;
        case GLSLDataType::UInt:   return 1;
        case GLSLDataType::Bool:   return 1;
        case GLSLDataType::Texture2D: return 0;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}

u32 Std140DataTypeSize( GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Float:  return 4;
        case GLSLDataType::Float2: return 4 * 2;
        case GLSLDataType::Float3: return 4 * 4;
        case GLSLDataType::Float4: return 4 * 4;
        // Three column-vectors, each padded to 16 bytes. This returned 64
        // before, which is the mat4 size; nothing used a mat3 in a uniform
        // block so it never showed up.
        case GLSLDataType::Mat3:   return 4 * 4 * 3;
        case GLSLDataType::Mat4:   return 4 * 4 * 4;
        case GLSLDataType::Int:    return 4;
        case GLSLDataType::Int2:   return 4 * 2;
        case GLSLDataType::Int3:   return 4 * 4;
        case GLSLDataType::Int4:   return 4 * 4;
        case GLSLDataType::UInt:   return 4;
        case GLSLDataType::Bool:   return 4;
        case GLSLDataType::Texture2D: return 0;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}

u32 Std140DataTypeAligment( GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Float:  return 4;
        case GLSLDataType::Float2: return 8;
        case GLSLDataType::Float3: return 16;
        case GLSLDataType::Float4: return 16;
        case GLSLDataType::Mat3:   return 16;
        case GLSLDataType::Mat4:   return 16;
        case GLSLDataType::Int:    return 4;
        case GLSLDataType::Int2:   return 8;
        case GLSLDataType::Int3:   return 16;
        case GLSLDataType::Int4:   return 16;
        case GLSLDataType::UInt:   return 4;
        case GLSLDataType::Bool:   return 4;
        case GLSLDataType::Texture2D: return 0;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}

wgpu::VertexFormat ToWGPUVertexFormat( GLSLDataType type )
{
    switch ( type )
    {
        case GLSLDataType::Float:  return wgpu::VertexFormat::Float32;
        case GLSLDataType::Float2: return wgpu::VertexFormat::Float32x2;
        case GLSLDataType::Float3: return wgpu::VertexFormat::Float32x3;
        case GLSLDataType::Float4: return wgpu::VertexFormat::Float32x4;
        case GLSLDataType::Int:    return wgpu::VertexFormat::Sint32;
        case GLSLDataType::Int2:   return wgpu::VertexFormat::Sint32x2;
        case GLSLDataType::Int3:   return wgpu::VertexFormat::Sint32x3;
        case GLSLDataType::Int4:   return wgpu::VertexFormat::Sint32x4;
        case GLSLDataType::UInt:   return wgpu::VertexFormat::Uint32;
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
    const auto add = [&]( VertexAttributeSemantic semantic, GLSLDataType type, u64 count )
    {
        // The whole point: an attribute with no data produces no slot, so no
        // pipeline ever declares one pointing outside the buffer.
        if ( count == 0 )
            return;

        const u64 size = (u64)GLSLDataTypeSize( type ) * count;
        layout.mSlots.push_back( VertexAttributeSlot{ semantic, type, offset, size } );
        offset += size;
    };

    add( VertexAttributeSemantic::Position,  GLSLDataType::Float3, vbd.mPositions.size() );
    add( VertexAttributeSemantic::Normal,    GLSLDataType::Float3, vbd.mNormals.size() );
    add( VertexAttributeSemantic::TexCoords, GLSLDataType::Float2, vbd.mTexCoords.size() );
    add( VertexAttributeSemantic::Tangent,   GLSLDataType::Float3, vbd.mTangents.size() );
    add( VertexAttributeSemantic::Bitangent, GLSLDataType::Float3, vbd.mBitangents.size() );

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
        bufferLayout.arrayStride = GLSLDataTypeSize( slot.mType );
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

    pass.setIndexBuffer( *mIndexBuffer, wgpu::IndexFormat::Uint32,
                         0, mIndexCount * sizeof( u32 ) );
}


// ---------------------------------------------------------------------------
// VertexBufferLayout
// ---------------------------------------------------------------------------

VertexBufferLayout::VertexBufferLayout( const std::initializer_list<BufferElement>& elements )
    : mElements( elements )
{
    CalculateOffsetsAndStride();
}

void VertexBufferLayout::CalculateOffsetsAndStride()
{
    const auto alignTo = []( u64 offset, u64 alignment )
    {
        return ( offset + alignment - 1 ) & ~( alignment - 1 );
    };

    u64 offset = 0;
    for ( auto& element : mElements )
    {
        offset = alignTo( offset, Std140DataTypeAligment( element.mType ) );
        element.mOffset = offset;
        element.mSize = Std140DataTypeSize( element.mType );
        offset += element.mSize * element.mCount;
    }
    // A uniform block's stride is rounded up to a vec4.
    mStride = alignTo( offset, 16 );
}

void VertexBufferLayout::SetStride( u64 stride )
{
    mStride = stride;
}

u64 VertexBufferLayout::Stride() const
{
    return mStride;
}

u64 VertexBufferLayout::Size() const
{
    return mElements.size();
}

const vector<BufferElement>& VertexBufferLayout::Elements() const
{
    return mElements;
}

vector<BufferElement>::iterator VertexBufferLayout::begin()
{
    return mElements.begin();
}

vector<BufferElement>::iterator VertexBufferLayout::end()
{
    return mElements.end();
}

vector<BufferElement>::const_iterator VertexBufferLayout::begin() const
{
    return mElements.begin();
}

vector<BufferElement>::const_iterator VertexBufferLayout::end() const
{
    return mElements.end();
}


// ---------------------------------------------------------------------------
// UniformBuffer
// ---------------------------------------------------------------------------

UniformBuffer::UniformBuffer( i32 index, string name, const VertexBufferLayout& layout, u32 size )
    : mName( std::move( name ) ),
      mLayout( layout ),
      mSize( size ),
      mIndex( (u64)index )
{
    CalculateOffsetsAndStride();
    mStaging.assign( mBufferSize, 0 );
    mBuffer = CreateBuffer( mBufferSize, wgpu::BufferUsage::Uniform, mName );
}

UniformBuffer::UniformBuffer( UniformBuffer&& other ) noexcept
{
    Swap( other );
}

UniformBuffer& UniformBuffer::operator=( UniformBuffer&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

void UniformBuffer::Swap( UniformBuffer& other ) noexcept
{
    std::swap( mBuffer, other.mBuffer );
    std::swap( mStaging, other.mStaging );
    std::swap( mName, other.mName );
    std::swap( mLayout, other.mLayout );
    std::swap( mBufferSize, other.mBufferSize );
    std::swap( mSize, other.mSize );
    std::swap( mIndex, other.mIndex );
}

void UniformBuffer::CalculateOffsetsAndStride()
{
    mBufferSize = Align4( mLayout.Stride() * mSize );
}

void UniformBuffer::SetData( const void* data, u32 size, u32 offset )
{
    BUBBLE_ASSERT( offset + size <= mStaging.size(), "Uniform buffer write out of range" );
    std::memcpy( mStaging.data() + offset, data, size );
}

void UniformBuffer::Flush( u64 elementCount )
{
    const u64 elements = elementCount == 0 ? mSize : std::min( elementCount, mSize );
    const u64 bytes = Align4( std::min( mLayout.Stride() * elements, (u64)mStaging.size() ) );
    if ( bytes == 0 )
        return;
    Gpu().Queue().writeBuffer( *mBuffer, 0, mStaging.data(), bytes );
}

const string& UniformBuffer::Name() const
{
    return mName;
}

u64 UniformBuffer::Index() const
{
    return mIndex;
}

UniformArrayElement UniformBuffer::operator[]( u64 index )
{
    return Element( index );
}

UniformArrayElement UniformBuffer::Element( u64 index )
{
    BUBBLE_ASSERT( index < mSize, "Buffer access valiation" );
    return UniformArrayElement( *this, index );
}

const VertexBufferLayout& UniformBuffer::Layout() const
{
    return mLayout;
}

u64 UniformBuffer::BufferSize() const
{
    return mBufferSize;
}

u64 UniformBuffer::Size() const
{
    return mSize;
}

u8* UniformBuffer::StagingAt( u64 offset )
{
    BUBBLE_ASSERT( offset < mStaging.size(), "Uniform buffer offset out of range" );
    return mStaging.data() + offset;
}


// ---------------------------------------------------------------------------
// UniformArrayElement
// ---------------------------------------------------------------------------

UniformArrayElement::UniformArrayElement( UniformBuffer& uniformBuffer, u64 index )
    : mBuffer( &uniformBuffer ),
      mArrayIndex( index )
{
}

void UniformArrayElement::SetData( const void* data, u64 size, u64 offset )
{
    const u64 stride = mBuffer->Layout().Stride();
    size = size ? size : stride;
    std::memcpy( mBuffer->StagingAt( stride * mArrayIndex + offset ), data, size );
}

void UniformArrayElement::SetInt( const string& name, i32 data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Int ), &data, sizeof( data ) );
}

void UniformArrayElement::SetUInt( const string& name, u32 data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::UInt ), &data, sizeof( data ) );
}

void UniformArrayElement::SetFloat( const string& name, f32 data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Float ), &data, sizeof( data ) );
}

void UniformArrayElement::SetFloat2( const string& name, const vec2& data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Float2 ), value_ptr( data ), sizeof( vec2 ) );
}

void UniformArrayElement::SetFloat3( const string& name, const vec3& data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Float3 ), value_ptr( data ), sizeof( vec3 ) );
}

void UniformArrayElement::SetFloat4( const string& name, const vec4& data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Float4 ), value_ptr( data ), sizeof( vec4 ) );
}

void UniformArrayElement::SetMat3( const string& name, const mat3& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Mat3 );
    // A mat3 is three column-vectors each padded to 16 bytes, so it cannot be
    // memcpy'd from a glm::mat3 in one go.
    u8* dst = mBuffer->StagingAt( mBuffer->Layout().Stride() * mArrayIndex + elem.mOffset );
    for ( i32 column = 0; column < 3; column++ )
        std::memcpy( dst + (u64)column * 16, value_ptr( data[column] ), sizeof( vec3 ) );
}

void UniformArrayElement::SetMat4( const string& name, const mat4& data )
{
    SetRawData( FindBufferElement( name, GLSLDataType::Mat4 ), value_ptr( data ), sizeof( mat4 ) );
}

const BufferElement& UniformArrayElement::FindBufferElement( const string& name, GLSLDataType type )
{
    const auto& layout = mBuffer->Layout();
    auto elem = std::find_if( layout.begin(), layout.end(),
                              [&name, &type]( const BufferElement& elem )
    {
        return elem.mName == name && elem.mType == type;
    } );
    BUBBLE_ASSERT( elem != layout.end(), "Uniform buffer element not founded" );
    return *elem;
}

void UniformArrayElement::SetRawData( const BufferElement& elem, const void* data, u64 size )
{
    const u64 offset = mBuffer->Layout().Stride() * mArrayIndex + elem.mOffset;
    std::memcpy( mBuffer->StagingAt( offset ), data, size );
}

}
