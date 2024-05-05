
#include "engine/renderer/buffer.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace bubble
{

u32 GLSLDataTypeSize( GLSLDataType type )
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

u32 GLSLDataComponentCount( GLSLDataType type )
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


u32 Std140DataTypeSize( GLSLDataType type )
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


u32 Std140DataTypeAligment( GLSLDataType type )
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


// Buffer layout
BufferLayout::BufferLayout( const std::initializer_list<BufferElement>& elements )
    : mElements( elements )
{
    CalculateOffsetsAndStride();
}

BufferLayout::~BufferLayout()
{
}

void BufferLayout::SetStride( u64 stride )
{
    mStride = stride;
}

u64 BufferLayout::Stride() const
{
    return mStride;
}

u64 BufferLayout::Size() const
{
    return mElements.size();
}

const vector<BufferElement>& BufferLayout::Elements() const
{
    return mElements;
}

vector<BufferElement>::iterator BufferLayout::begin()
{
    return mElements.begin();
}
vector<BufferElement>::iterator BufferLayout::end()
{
    return mElements.end();
}
vector<BufferElement>::const_iterator BufferLayout::begin() const
{
    return mElements.begin();
}
vector<BufferElement>::const_iterator BufferLayout::end() const
{
    return mElements.end();
}

void BufferLayout::CalculateOffsetsAndStride()
{
    u64 offset = 0;
    for ( auto& element : mElements )
    {
        element.mOffset = offset;
        element.mSize = GLSLDataTypeSize( element.mType );
        offset += element.mSize * element.mCount;
        mStride += u32( element.mSize );
    }
    // If count more then one, it means that
    // attributes goes one after another (1111 2222 3333)
    // So stride will be the size of each single attribute
    mStride = mElements[0].mCount == 1 ? mStride : 0;
}



// Vertex buffer 
VertexBuffer::VertexBuffer( const BufferLayout& layout, u64 size )
    : mLayout( layout ), 
      mSize( size )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW ) );
}

VertexBuffer::VertexBuffer( const BufferLayout& layout, void* vertices, u64 size )
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

void VertexBuffer::SetData( const void* data, u32 size )
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferSubData( GL_ARRAY_BUFFER, 0, size, data ) );
}

const BufferLayout& VertexBuffer::Layout() const
{
    return mLayout;
}

void VertexBuffer::SetLayout( const BufferLayout& layout )
{
    mLayout = layout;
}

u64 VertexBuffer::Size()
{
    return mSize;
}


// Index buffer
IndexBuffer::IndexBuffer( u32* indices, u64 count )
    : mCount( count )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ELEMENT_ARRAY_BUFFER, count * sizeof( u32 ), indices, GL_STATIC_DRAW ) );
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

u64 IndexBuffer::GetCount() const
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
    BUBBLE_ASSERT( vertexBuffer.Layout().Size(), "VertexBuffer has no layout!" );

    Bind();
    vertexBuffer.Bind();

    const auto& layout = vertexBuffer.Layout();
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
                    GLsizei( layout.Stride() ? layout.Stride() : element.mSize ),
                    (const void*)element.mOffset ) );
            VertexBufferIndex( mVertexBufferIndex + 1 );
        }break;
        case GLSLDataType::Mat3:
        case GLSLDataType::Mat4:
        {
            u32 count = GLSLDataComponentCount( element.mType );
            for ( u32 i = 0; i < count; i++ )
            {
                glcall( glEnableVertexAttribArray( mVertexBufferIndex ) );
                glcall( glVertexAttribPointer( mVertexBufferIndex,
                        count,
                        GLSLDataTypeToOpenGLBasemType( element.mType ),
                        element.mNormalized ? GL_TRUE : GL_FALSE,
                        GLsizei( layout.Stride() ? layout.Stride() : element.mSize ),
                        (const void*)( sizeof( f32 ) * count * i ) ) );
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

u32 VertexArray::RendererID() const
{
    return mRendererID;
}

vector<VertexBuffer>& VertexArray::GetVertexBuffers()
{
    return mVertexBuffers;
}

IndexBuffer& VertexArray::GetIndexBuffer()
{
    return mIndexBuffer;
}

void VertexArray::VertexBufferIndex( u32 val )
{
    mVertexBufferIndex = val;
}


// UniformBuffer 
UniformBuffer::UniformBuffer( i32 index, 
                              string name,
                              const BufferLayout& layout,
                              u32 size,
                              u32 additional_size )
    : mIndex( index ), 
      mName( std::move( name ) ),
      mLayout( layout ),
      mSize( size )
{
    CalculateOffsetsAndStride();
    mBufferSize = mLayout.Stride() * size + additional_size;

    glGenBuffers( 1, &mRendererID );
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferData( GL_UNIFORM_BUFFER, mBufferSize, NULL, GL_DYNAMIC_DRAW );

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

GLint UniformBuffer::RendererID() const
{
    return mRendererID;
}

const string& UniformBuffer::Name() const
{
    return mName;
}

u64 UniformBuffer::Index() const
{
    return mIndex;
}

void UniformBuffer::SetData( const void* data, u32 size, u32 offset )
{
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, offset, size, data );
}

UniformArrayElemnt UniformBuffer::operator[]( u64 index )
{
    return Element( index );
}

UniformArrayElemnt UniformBuffer::Element( u64 index )
{
    BUBBLE_ASSERT( index < mSize, "Buffer access valiation" );
    return UniformArrayElemnt( *this, index );
}

void UniformBuffer::CalculateOffsetsAndStride()
{
    u64 offset = 0;
    u64 additionalAlignment = 0; // std140 alignment

    for ( BufferElement& elemnt : mLayout )
    {
        u64 std140Aligment = Std140DataTypeAligment( elemnt.mType );
        elemnt.mSize = Std140DataTypeSize( elemnt.mType );

        auto notFilledSpace = offset % std140Aligment;
        if( notFilledSpace )
            additionalAlignment = std140Aligment - notFilledSpace;

        elemnt.mOffset = offset + additionalAlignment;
        offset += elemnt.mSize + additionalAlignment;
    }
    // Align by vec4 size
    u64 vec4Size = Std140DataTypeSize( GLSLDataType::Float4 );
    
    auto notFilledSpace = offset % vec4Size;
    if( notFilledSpace )
        offset += vec4Size - notFilledSpace;

    mLayout.SetStride( offset );
}

const BufferLayout& UniformBuffer::Layout() const
{
    return mLayout;
};

u64 UniformBuffer::BufferSize()
{
    return mBufferSize;
}
u64 UniformBuffer::Size()
{
    return mSize;
};

// UniformArrayElemnt 
UniformArrayElemnt::UniformArrayElemnt( const UniformBuffer& uniform_buffer, u64 index )
    : mLayout( uniform_buffer.Layout() ),
      mRendererID( uniform_buffer.RendererID() ),
      mArrayIndex( index )
{
}

void UniformArrayElemnt::SetData( const void* data, u64 size, u64 offset )
{
    u64 array_index_offset = mLayout.Stride() * mArrayIndex;
    size = size ? size : mLayout.Stride();
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset + offset, size, data );
}

void UniformArrayElemnt::SetInt( const string& name, i32 data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Int );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat( const string& name, f32 data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat2( const string& name, const vec2& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float2 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat3( const string& name, const vec3& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float3 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat4( const string& name, const vec4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float4 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElemnt::SetMat4( const string& name, const mat4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Mat4 );
    SetRawData( elem, value_ptr( data ) );
}


const BufferElement& UniformArrayElemnt::FindBufferElement( const string& name, GLSLDataType type )
{
    auto elem = std::find_if( mLayout.begin(), mLayout.end(),
                              [&name, &type]( const BufferElement& elem )
    {
        return elem.mName == name && elem.mType == type;
    } );
    BUBBLE_ASSERT( elem != mLayout.end(), "Uniform buffer element not founded" );
    return *elem;
}

void UniformArrayElemnt::SetRawData( const BufferElement& elem, const void* data )
{
    u64 array_index_offset = mLayout.Stride() * mArrayIndex + elem.mOffset;
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset, elem.mSize, data );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

}
