
#include "renderer/buffer.hpp"

namespace bubble
{

uint32_t GLSLDataTypeSize( GLSLDataType type )
{
    switch( type )
    {
    case GLSLDataType::Float:
        return sizeof( GLfloat );
        return sizeof( GLfloat ) * 2;
    case GLSLDataType::Float3:
        return sizeof( GLfloat ) * 3;
    case GLSLDataType::Float4:
        return sizeof( GLfloat ) * 4;
    case GLSLDataType::Mat3:
        return sizeof( GLfloat ) * 3 * 3;
    case GLSLDataType::Mat4:
        return sizeof( GLfloat ) * 4 * 4;
    case GLSLDataType::Int:
        return sizeof( GLint );
    case GLSLDataType::Int2:
        return sizeof( GLint ) * 2;
    case GLSLDataType::Int3:
        return sizeof( GLint ) * 3;
    case GLSLDataType::Int4:
        return sizeof( GLint ) * 4;
    case GLSLDataType::Bool:
        return sizeof( GLboolean );
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}

uint32_t GLSLDataComponentCount( GLSLDataType type )
{
    switch( type )
    {
    case GLSLDataType::Float:
        return 1;
    case GLSLDataType::Float2:
        return 2;
    case GLSLDataType::Float3:
        return 3;
    case GLSLDataType::Float4:
        return 4;
    case GLSLDataType::Mat3:
        return 3;
    case GLSLDataType::Mat4:
        return 4;
    case GLSLDataType::Int:
        return 1;
    case GLSLDataType::Int2:
        return 2;
    case GLSLDataType::Int3:
        return 3;
    case GLSLDataType::Int4:
        return 4;
    case GLSLDataType::Bool:
        return 1;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}


// BufferElement
BufferElement::BufferElement( GLSLDataType type,
               const std::string& name,
               size_t count,
               bool normalized )
    : mName( name ),
    mType( type ),
    mSize( GLSLDataTypeSize( type ) ),
    mOffset( 0 ),
    mCount( count ),
    mNormalized( normalized )
{
}

// BufferLayout
BufferLayout::BufferLayout( const std::initializer_list<BufferElement>& elements )
    : mElements( elements )
{
    CalculateOffsetsAndStride();
}

size_t BufferLayout::GetStride() const
{
    return mStride;
}
const std::vector<BufferElement>& BufferLayout::GetElements() const
{
    return mElements;
}

std::vector<BufferElement>::iterator BufferLayout::begin()
{
    return mElements.begin();
}
std::vector<BufferElement>::iterator BufferLayout::end()
{
    return mElements.end();
}
std::vector<BufferElement>::const_iterator BufferLayout::begin() const
{
    return mElements.begin();
}
std::vector<BufferElement>::const_iterator BufferLayout::end() const
{
    return mElements.end();
}

void BufferLayout::CalculateOffsetsAndStride()
{
    size_t offset = 0;
    for( auto& element : mElements )
    {
        element.mOffset = offset;
        offset += element.mSize * element.mCount;
        mStride += uint32_t( element.mSize );
    }
    // If count more then one, it means that
    // attributes goes one after another (1111 2222 3333)
    // So stride will be the size of each single attribute
    mStride = mElements[0].mCount == 1 ? mStride : 0;
}



// Vertex buffer 
VertexBuffer::VertexBuffer( size_t size )
    : mSize( size )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW ) );
}

VertexBuffer::VertexBuffer( void* vertices, size_t size )
    : mSize( size )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW ) );
}

VertexBuffer::VertexBuffer( VertexBuffer&& other ) noexcept
{
    mRendererID = other.mRendererID;
    mLayout = std::move( other.mLayout );
    mSize = other.mSize;
    other.mRendererID = 0;
}

VertexBuffer& VertexBuffer::operator=( VertexBuffer&& other ) noexcept
{
    if( this != &other )
    {
        mRendererID = other.mRendererID;
        mLayout = std::move( other.mLayout );
        mSize = other.mSize;
        other.mRendererID = 0;
        other.mSize = 0;
    }
    return *this;
}

VertexBuffer::~VertexBuffer()
{
    glcall( glDeleteBuffers( 1, &mRendererID ) );
}

void VertexBuffer::Bind() const
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
}

void VertexBuffer::Unbind() const
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
}

void VertexBuffer::SetData( const void* data, uint32_t size )
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferSubData( GL_ARRAY_BUFFER, 0, size, data ) );
}

const bubble::BufferLayout& VertexBuffer::GetLayout() const
{
    return mLayout;
}

void VertexBuffer::SetLayout( const BufferLayout& layout )
{
    mLayout = layout;
}

size_t VertexBuffer::GetSize()
{
    return mSize;
}


// Index buffer
IndexBuffer::IndexBuffer( uint32_t* indices, size_t count )
    : mCount( count )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ELEMENT_ARRAY_BUFFER, count * sizeof( uint32_t ), indices, GL_STATIC_DRAW ) );
}

IndexBuffer::IndexBuffer( IndexBuffer&& other ) noexcept
{
    mRendererID = other.mRendererID;
    mCount = other.mCount;
    other.mRendererID = 0;
    other.mCount = 0;
}

IndexBuffer& IndexBuffer::operator=( IndexBuffer&& other ) noexcept
{
    if( this != &other )
    {
        mRendererID = other.mRendererID;
        mCount = other.mCount;
        other.mRendererID = 0;
        other.mCount = 0;
    }
    return *this;
}

IndexBuffer::~IndexBuffer()
{
    glcall( glDeleteBuffers( 1, &mRendererID ) );
}

void IndexBuffer::Bind() const
{
    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mRendererID ) );
}

void IndexBuffer::Unbind() const
{
    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );
}

size_t IndexBuffer::GetCount() const
{
    return mCount;
}

}
