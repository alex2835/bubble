
#include "engine/renderer/buffer.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace bubble
{

uint32_t GLSLDataTypeSize( GLSLDataType type )
{
    switch ( type )
    {
    case GLSLDataType::Float:
        return sizeof( GLfloat );
    case GLSLDataType::Float2:
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
    switch ( type )
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

GLenum GLSLDataTypeToOpenGLBasemType( GLSLDataType mType )
{
    switch ( mType )
    {
    case GLSLDataType::Float:
        return GL_FLOAT;
    case GLSLDataType::Float2:
        return GL_FLOAT;
    case GLSLDataType::Float3:
        return GL_FLOAT;
    case GLSLDataType::Float4:
        return GL_FLOAT;
    case GLSLDataType::Mat3:
        return GL_FLOAT;
    case GLSLDataType::Mat4:
        return GL_FLOAT;
    case GLSLDataType::Int:
        return GL_INT;
    case GLSLDataType::Int2:
        return GL_INT;
    case GLSLDataType::Int3:
        return GL_INT;
    case GLSLDataType::Int4:
        return GL_INT;
    case GLSLDataType::Bool:
        return GL_BOOL;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}


uint32_t Std140DataTypeSize( GLSLDataType type )
{
    switch ( type )
    {
    case GLSLDataType::Float:
        return 4;
    case GLSLDataType::Float2:
        return 4 * 2;
    case GLSLDataType::Float3:
        return 4 * 4;
    case GLSLDataType::Float4:
        return 4 * 4;
    case GLSLDataType::Mat3:
        return 4 * 4 * 4;
    case GLSLDataType::Mat4:
        return 4 * 4 * 4;
    case GLSLDataType::Int:
        return 4;
    case GLSLDataType::Int2:
        return 4 * 2;
    case GLSLDataType::Int3:
        return 4 * 4;
    case GLSLDataType::Int4:
        return 4 * 4;
    case GLSLDataType::Bool:
        return 4;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}


uint32_t Std140DataTypePadding( GLSLDataType type )
{
    switch ( type )
    {
    case GLSLDataType::Float:
        return 4;
    case GLSLDataType::Float2:
        return 8;
    case GLSLDataType::Float3:
        return 16;
    case GLSLDataType::Float4:
        return 16;
    case GLSLDataType::Mat3:
        return 16;
    case GLSLDataType::Mat4:
        return 16;
    case GLSLDataType::Int:
        return 4;
    case GLSLDataType::Int2:
        return 8;
    case GLSLDataType::Int3:
        return 16;
    case GLSLDataType::Int4:
        return 16;
    case GLSLDataType::Bool:
        return 4;
    }
    BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
    return 0;
}



//// BufferElement
//BufferElement::BufferElement( GLSLDataType type,
//                              const std::string& name,
//                              size_t count,
//                              bool normalized )
//    : mName( name ),
//      mType( type ),
//      mSize( GLSLDataTypeSize( type ) ),
//      mOffset( 0 ),
//      mCount( count ),
//      mNormalized( normalized )
//{
//}

// BufferLayout
BufferLayout::BufferLayout( const std::initializer_list<BufferElement>& elements )
    : mElements( elements )
{
    CalculateOffsetsAndStride();
}

BufferLayout::~BufferLayout()
{
}

size_t BufferLayout::GetStride() const
{
    return mStride;
}

size_t BufferLayout::Size() const
{
    return mElements.size();
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
    for ( auto& element : mElements )
    {
        element.mOffset = offset;
        element.mSize = GLSLDataTypeSize( element.mType );
        offset += element.mSize * element.mCount;
        mStride += uint32_t( element.mSize );
    }
    // If count more then one, it means that
    // attributes goes one after another (1111 2222 3333)
    // So stride will be the size of each single attribute
    mStride = mElements[0].mCount == 1 ? mStride : 0;
}



// Vertex buffer 
VertexBuffer::VertexBuffer( const BufferLayout& layout, size_t size )
    : mLayout( layout ), 
      mSize( size )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW ) );
}

VertexBuffer::VertexBuffer( const BufferLayout& layout, void* vertices, size_t size )
    : mLayout( layout ),
      mSize( size )
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
    if ( this != &other )
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

const BufferLayout& VertexBuffer::GetLayout() const
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
    if ( this != &other )
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


// VertexArray
VertexArray::VertexArray() noexcept
{
    glcall( glGenVertexArrays( 1, &mRendererID ) );
    Bind();
}

VertexArray::VertexArray( VertexArray&& other ) noexcept
{
    mRendererID = other.mRendererID;
    VertexBufferIndex( other.mVertexBufferIndex );
    mVertexBuffers = std::move( other.mVertexBuffers );
    mIndexBuffer = std::move( other.mIndexBuffer );
    other.mRendererID = 0;
    other.VertexBufferIndex( 0 );
}

VertexArray& VertexArray::operator=( VertexArray&& other ) noexcept
{
    if ( this != &other )
    {
        mRendererID = other.mRendererID;
        VertexBufferIndex( other.mVertexBufferIndex );
        mVertexBuffers = std::move( other.mVertexBuffers );
        mIndexBuffer = std::move( other.mIndexBuffer );
        other.mRendererID = 0;
        other.VertexBufferIndex( 0 );
    }
    return *this;
}

VertexArray::~VertexArray()
{
    glcall( glDeleteVertexArrays( 1, &mRendererID ) );
}

void VertexArray::Bind() const
{
    glcall( glBindVertexArray( mRendererID ) );
}

void VertexArray::Unbind() const
{
    glcall( glBindVertexArray( 0 ) );
}

void VertexArray::AddVertexBuffer( VertexBuffer&& vertexBuffer )
{
    BUBBLE_ASSERT( vertexBuffer.GetLayout().Size(), "VertexBuffer has no layout!" );

    Bind();
    vertexBuffer.Bind();

    const auto& layout = vertexBuffer.GetLayout();
    for ( const auto& element : layout )
    {
        switch ( element.mType )
        {
        case GLSLDataType::Float:
        case GLSLDataType::Float2:
        case GLSLDataType::Float3:
        case GLSLDataType::Float4:
        case GLSLDataType::Int:
        case GLSLDataType::Int2:
        case GLSLDataType::Int3:
        case GLSLDataType::Int4:
        case GLSLDataType::Bool:
        {
            glcall( glEnableVertexAttribArray( mVertexBufferIndex ) );
            glcall( glVertexAttribPointer( mVertexBufferIndex,
                    GLSLDataComponentCount( element.mType ),
                    GLSLDataTypeToOpenGLBasemType( element.mType ),
                    element.mNormalized ? GL_TRUE : GL_FALSE,
                    GLsizei( layout.GetStride() ? layout.GetStride() : element.mSize ),
                    (const void*)element.mOffset ) );
            VertexBufferIndex( mVertexBufferIndex + 1 );
        }break;
        case GLSLDataType::Mat3:
        case GLSLDataType::Mat4:
        {
            uint32_t count = GLSLDataComponentCount( element.mType );
            for ( uint32_t i = 0; i < count; i++ )
            {
                glcall( glEnableVertexAttribArray( mVertexBufferIndex ) );
                glcall( glVertexAttribPointer( mVertexBufferIndex,
                        count,
                        GLSLDataTypeToOpenGLBasemType( element.mType ),
                        element.mNormalized ? GL_TRUE : GL_FALSE,
                        GLsizei( layout.GetStride() ? layout.GetStride() : element.mSize ),
                        (const void*)( sizeof( float ) * count * i ) ) );
                glcall( glVertexAttribDivisor( mVertexBufferIndex, 1 ) );
                VertexBufferIndex( mVertexBufferIndex + 1 );
            }
        }break;
        default:
        {
            BUBBLE_ASSERT( false, "Unknown GLSLDataType!" );
        }
        }
    }
    mVertexBuffers.push_back( std::move( vertexBuffer ) );
    Unbind();
}

void VertexArray::SetIndexBuffer( IndexBuffer&& indexBuffer )
{
    Bind();
    indexBuffer.Bind();
    mIndexBuffer = std::move( indexBuffer );
    Unbind();
}

uint32_t VertexArray::GetRendererID() const
{
    return mRendererID;
}

std::vector<VertexBuffer>& VertexArray::GetVertexBuffers()
{
    return mVertexBuffers;
}

IndexBuffer& VertexArray::GetIndexBuffer()
{
    return mIndexBuffer;
}

void VertexArray::VertexBufferIndex( uint32_t val )
{
    mVertexBufferIndex = val;
}


// UniformBuffer 
UniformBuffer::UniformBuffer( int index,
                              const BufferLayout& layout,
                              uint32_t size,
                              uint32_t additional_size )
    : mLayout( layout ),
      mIndex( index ),
      mSize( size )
{
    CalculateOffsetsAndStride();
    mBufferSize = mLayout.mStride * size + additional_size;

    glGenBuffers( 1, &mRendererID );
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferData( GL_UNIFORM_BUFFER, mBufferSize, NULL, GL_STATIC_DRAW );

    glBindBufferRange( GL_UNIFORM_BUFFER, index, mRendererID, 0, mBufferSize );
}

UniformBuffer::UniformBuffer( UniformBuffer&& other ) noexcept
    : mRendererID( other.mRendererID ),
    mBufferSize( other.mBufferSize ),
    mLayout( std::move( other.mLayout ) ),
    mIndex( other.mIndex ),
    mSize( other.mSize )
{
    other.mRendererID = 0;
    other.mSize = 0;
    other.mBufferSize = 0;
}

UniformBuffer& UniformBuffer::operator=( UniformBuffer&& other ) noexcept
{
    if ( this != &other )
    {
        mRendererID = other.mRendererID;
        mBufferSize = other.mBufferSize;
        mLayout = std::move( other.mLayout );
        mIndex = other.mIndex;
        mSize = other.mSize;

        other.mRendererID = 0;
        other.mSize = 0;
        other.mBufferSize = 0;
    }
    return *this;
}

UniformBuffer::~UniformBuffer()
{
    glDeleteBuffers( 1, &mRendererID );
}

void UniformBuffer::SetData( const void* data, uint32_t size, uint32_t offset )
{
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, offset, size, data );
}

UniformArrayElemnt UniformBuffer::operator[]( int index )
{
    BUBBLE_ASSERT( *(uint32_t*)&index < mSize, "Buffer acess valiation" );
    return UniformArrayElemnt( *this, index );
}

void UniformBuffer::CalculateOffsetsAndStride()
{
    size_t offset = 0;
    size_t pad = 0;  // padding in std140

    for ( BufferElement& elemnt : mLayout )
    {
        size_t std140_pad = Std140DataTypePadding( elemnt.mType );
        elemnt.mSize = Std140DataTypeSize( elemnt.mType );

        pad = offset % std140_pad;
        pad = pad > 0 ? std140_pad - pad : 0;

        elemnt.mOffset = offset + pad;
        offset += elemnt.mSize + pad;
    }
    // Align by vec4 size
    size_t vec4_size = Std140DataTypeSize( GLSLDataType::Float4 );
    pad = offset % vec4_size;
    offset += pad > 0 ? vec4_size - pad : 0;

    mLayout.mStride = offset;
}

const BufferLayout& UniformBuffer::GetLayout() const
{
    return mLayout;
};

size_t UniformBuffer::GetBufferSize()
{
    return mBufferSize;
}
size_t UniformBuffer::GetSize()
{
    return mSize;
};

// UniformArrayElemnt 
UniformArrayElemnt::UniformArrayElemnt( const UniformBuffer& uniform_buffer, int index )
    : mLayout( &uniform_buffer.mLayout ),
      mRendererID( uniform_buffer.mRendererID ),
      mArrayIndex( index )
{
}

void UniformArrayElemnt::SetData( const void* data, size_t size, uint32_t offset )
{
    size_t array_index_offset = mLayout->mStride * mArrayIndex;
    size = size ? size : mLayout->mStride;
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset + offset, size, data );
}


void UniformArrayElemnt::SetInt( const std::string& name, int data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Int );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat( const std::string& name, float data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat2( const std::string& name, const glm::vec2& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float2 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat3( const std::string& name, const glm::vec3& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float3 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat4( const std::string& name, const glm::vec4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float4 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetMat4( const std::string& name, const glm::mat4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Mat4 );
    SetRawData( elem, glm::value_ptr( data ) );
}


const BufferElement& UniformArrayElemnt::FindBufferElement( const std::string& name, GLSLDataType type )
{
    auto elem = std::find_if( mLayout->begin(), mLayout->end(),
                              [&name, &type]( const BufferElement& elem )
    {
        return elem.mName == name && elem.mType == type;
    } );
    BUBBLE_ASSERT( elem != mLayout->end(), "Uniform buffer element not founded" );
    return *elem;
}

void UniformArrayElemnt::SetRawData( const BufferElement& elem, const void* data )
{
    size_t array_index_offset = mLayout->mStride * mArrayIndex + elem.mOffset;
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset, elem.mSize, data );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

}
