
#include <GL/glew.h>
#include "glm/gtc/type_ptr.hpp"
#include "engine/types/array.hpp"
#include "engine/renderer/buffer.hpp"

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
        return sizeof( i32 );
    case GLSLDataType::Int2:
        return sizeof( i32 ) * 2;
    case GLSLDataType::Int3:
        return sizeof( i32 ) * 3;
    case GLSLDataType::Int4:
        return sizeof( i32 ) * 4;
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

i32 GLSLDataTypeToOpenGLBasemType( GLSLDataType mType )
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


auto VerteBufferDataLayout( const VertexBufferData& vbd )
{
    return VertexBufferLayout
    {
        { "Position",  GLSLDataType::Float3, vbd.mPositions.size()  },
        { "Normal",    GLSLDataType::Float3, vbd.mNormals.size()    },
        { "TexCoords", GLSLDataType::Float2, vbd.mTexCoords.size()  },
        { "Tangent",   GLSLDataType::Float3, vbd.mTangents.size()   },
        { "Bitangent", GLSLDataType::Float3, vbd.mBitangents.size() }
    };
}

vector<u8> VertexBufferDataFlat( const VertexBufferData& vbd )
{
    u64 size = sizeof( vec3 ) * vbd.mPositions.size() +
        sizeof( vec3 ) * vbd.mNormals.size() +
        sizeof( vec2 ) * vbd.mTexCoords.size() +
        sizeof( vec3 ) * vbd.mTangents.size() +
        sizeof( vec3 ) * vbd.mBitangents.size();

    vector<u8> data( size );
    u64 offset = 0;
    memmove( data.data(), vbd.mPositions.data(), sizeof( vec3 ) * vbd.mPositions.size() );
    offset += sizeof( vec3 ) * vbd.mPositions.size();

    memmove( data.data() + offset, vbd.mNormals.data(), sizeof( vec3 ) * vbd.mNormals.size() );
    offset += sizeof( vec3 ) * vbd.mNormals.size();

    memmove( data.data() + offset, vbd.mTexCoords.data(), sizeof( vec2 ) * vbd.mTexCoords.size() );
    offset += sizeof( vec2 ) * vbd.mTexCoords.size();

    memmove( data.data() + offset, vbd.mTangents.data(), sizeof( vec3 ) * vbd.mTangents.size() );
    offset += sizeof( vec3 ) * vbd.mTangents.size();

    memmove( data.data() + offset, vbd.mBitangents.data(), sizeof( vec3 ) * vbd.mBitangents.size() );

    return data;
}



// Buffer layout
VertexBufferLayout::VertexBufferLayout( const std::initializer_list<BufferElement>& elements )
    : mElements( elements )
{
    CalculateOffsetsAndStride();
}

VertexBufferLayout::~VertexBufferLayout()
{
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

void VertexBufferLayout::CalculateOffsetsAndStride()
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
VertexBuffer::VertexBuffer( const VertexBufferLayout& layout, u64 size )
    : mLayout( layout ), 
      mSize( size ),
      mType( BufferType::Dynamic )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW ) );
}

VertexBuffer::VertexBuffer( VertexBuffer&& other ) noexcept
{
    Swap( other );
}

VertexBuffer::VertexBuffer( const VertexBufferData& vbd, BufferType type )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    Reallocate( vbd, type );
}

VertexBuffer& VertexBuffer::operator=( VertexBuffer&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

VertexBuffer::~VertexBuffer()
{
    glcall( glDeleteBuffers( 1, &mRendererID ) );
}

void VertexBuffer::Swap( VertexBuffer& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mSize, other.mSize );
    std::swap( mLayout, other.mLayout );
    std::swap( mType, other.mType );
}

void VertexBuffer::Bind() const
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
}

void VertexBuffer::Unbind() const
{
    glcall( glBindBuffer( GL_ARRAY_BUFFER, 0 ) );
}

void VertexBuffer::SetData( const vector<u8>& data )
{
    BUBBLE_ASSERT( mType == BufferType::Dynamic, "Set vertex buffer data works only with Dynamic buffers" );
    BUBBLE_ASSERT( data.size() == mSize, "Set data of different size is forbidden" );

    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferSubData( GL_ARRAY_BUFFER, 0, data.size(), data.data() ) );
}

void VertexBuffer::Reallocate( const VertexBufferData& vbd, BufferType type )
{
    mLayout = VerteBufferDataLayout( vbd );
    auto data = VertexBufferDataFlat( vbd );
    mSize = data.size();
    mType = type;
    glcall( glBindBuffer( GL_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ARRAY_BUFFER, data.size(), data.data(), (GLenum)type ) );
}

const VertexBufferLayout& VertexBuffer::Layout() const
{
    return mLayout;
}

u64 VertexBuffer::Size()
{
    return mSize;
}


// Index buffer
IndexBuffer::IndexBuffer( const vector<u32>& indices, BufferType type )
    : mCount( indices.size() ),
      mType( type )
{
    glcall( glGenBuffers( 1, &mRendererID ) );
    Realocate( indices, mType );
}

IndexBuffer::IndexBuffer( IndexBuffer&& other ) noexcept
{
    Swap( other );
}

IndexBuffer& IndexBuffer::operator=( IndexBuffer&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

IndexBuffer::~IndexBuffer()
{
    glcall( glDeleteBuffers( 1, &mRendererID ) );
}

void IndexBuffer::Swap( IndexBuffer& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mCount, other.mCount );
}

void IndexBuffer::SetData( const vector<u32>& indices )
{
    BUBBLE_ASSERT( mType == BufferType::Dynamic, "Set vertex buffer data works only with Dynamic buffers" );
    BUBBLE_ASSERT( indices.size() == mCount, "Set data of different size is forbidden" );

    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof( u32 ), indices.data() ) );
}

void IndexBuffer::Realocate( const vector<u32>& indices, BufferType type )
{
    mType = type;
    glcall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mRendererID ) );
    glcall( glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof( u32 ), indices.data(), (GLenum)type ) );
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
    Swap( other );
}

VertexArray& VertexArray::operator=( VertexArray&& other ) noexcept
{
    if ( this != &other )
        Swap( other );
    return *this;
}

VertexArray::~VertexArray()
{
    glcall( glDeleteVertexArrays( 1, &mRendererID ) );
}

void VertexArray::Swap( VertexArray& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mVertexBufferIndex, other.mVertexBufferIndex );
    std::swap( mVertexBuffer, other.mVertexBuffer );
    std::swap( mIndexBuffer, other.mIndexBuffer );
}

void VertexArray::Bind() const
{
    glcall( glBindVertexArray( mRendererID ) );
}

void VertexArray::Unbind() const
{
    glcall( glBindVertexArray( 0 ) );
}

void VertexArray::SetVertexBuffer( VertexBuffer&& vertexBuffer )
{
    Bind();
    BUBBLE_ASSERT( vertexBuffer.Layout().Size(), "VertexBuffer has no layout!" );
    vertexBuffer.Bind();
    mVertexBuffer = std::move( vertexBuffer );
    UpdateVerteBufferLayout();
    Unbind();
}

void VertexArray::SetIndexBuffer( IndexBuffer&& indexBuffer )
{
    Bind();
    indexBuffer.Bind();
    mIndexBuffer = std::move( indexBuffer );
    Unbind();
}

void VertexArray::SetBufferData( const VertexBufferData& vbd,
                                 const vector<u32>& indices,
                                 BufferType type )
{
    Bind();
    auto layout = VerteBufferDataLayout( vbd );
    if ( mVertexBuffer.Layout() == layout and
         mIndexBuffer.GetCount() == indices.size() )
    {
        mVertexBuffer.SetData( VertexBufferDataFlat( vbd ) );
        mIndexBuffer.SetData( indices );
    }
    else
    {
        mVertexBuffer.Reallocate( vbd, type );
        mIndexBuffer.Realocate( indices, type );
        UpdateVerteBufferLayout();
    }
    Unbind();
}

void VertexArray::UpdateVerteBufferLayout()
{
    VertexBufferIndex( 0 );
    const auto& layout = mVertexBuffer.Layout();
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
                                               i32( layout.Stride() ? layout.Stride() : element.mSize ),
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
                                                   i32( layout.Stride() ? layout.Stride() : element.mSize ),
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
}


u32 VertexArray::RendererID() const
{
    return mRendererID;
}

VertexBuffer& VertexArray::GetVertexBuffer()
{
    return mVertexBuffer;
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
                              const VertexBufferLayout& layout,
                              u32 size )
    : mName( std::move( name ) ),
      mLayout( layout ),
      mSize( size ),
      mIndex( index )
{
    CalculateOffsetsAndStride();
    mBufferSize = mLayout.Stride() * size;

    glGenBuffers( 1, &mRendererID );
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferData( GL_UNIFORM_BUFFER, mBufferSize, NULL, GL_DYNAMIC_DRAW );

    glBindBufferRange( GL_UNIFORM_BUFFER, index, mRendererID, 0, mBufferSize );
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

UniformBuffer::~UniformBuffer()
{
    glDeleteBuffers( 1, &mRendererID );
}

void UniformBuffer::Swap( UniformBuffer& other ) noexcept
{
    std::swap( mRendererID, other.mRendererID );
    std::swap( mName, other.mName );
    std::swap( mLayout, other.mLayout );
    std::swap( mBufferSize, other.mBufferSize );
    std::swap( mSize, other.mSize );
    std::swap( mIndex, other.mIndex );
}

i32 UniformBuffer::RendererID() const
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

UniformArrayElement UniformBuffer::operator[]( u64 index )
{
    return Element( index );
}

UniformArrayElement UniformBuffer::Element( u64 index )
{
    BUBBLE_ASSERT( index < mSize, "Buffer access valiation" );
    return UniformArrayElement( *this, index );
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

const VertexBufferLayout& UniformBuffer::Layout() const
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

// UniformArrayElement 
UniformArrayElement::UniformArrayElement( const UniformBuffer& uniform_buffer, u64 index )
    : mRendererID( uniform_buffer.RendererID() ),
      mArrayIndex( index ),
      mLayout( uniform_buffer.Layout() )
{
}

void UniformArrayElement::SetData( const void* data, u64 size, u64 offset )
{
    u64 array_index_offset = mLayout.Stride() * mArrayIndex;
    size = size ? size : mLayout.Stride();
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset + offset, size, data );
}

void UniformArrayElement::SetInt( const string& name, i32 data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Int );
    SetRawData( elem, &data );
}


void UniformArrayElement::SetFloat( const string& name, f32 data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float );
    SetRawData( elem, &data );
}


void UniformArrayElement::SetFloat2( const string& name, const vec2& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float2 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElement::SetFloat3( const string& name, const vec3& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float3 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElement::SetFloat4( const string& name, const vec4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Float4 );
    SetRawData( elem, value_ptr( data ) );
}


void UniformArrayElement::SetMat4( const string& name, const mat4& data )
{
    const BufferElement& elem = FindBufferElement( name, GLSLDataType::Mat4 );
    SetRawData( elem, value_ptr( data ) );
}


const BufferElement& UniformArrayElement::FindBufferElement( const string& name, GLSLDataType type )
{
    auto elem = std::find_if( mLayout.begin(), mLayout.end(),
                              [&name, &type]( const BufferElement& elem )
    {
        return elem.mName == name && elem.mType == type;
    } );
    BUBBLE_ASSERT( elem != mLayout.end(), "Uniform buffer element not founded" );
    return *elem;
}

void UniformArrayElement::SetRawData( const BufferElement& elem, const void* data )
{
    u64 array_index_offset = mLayout.Stride() * mArrayIndex + elem.mOffset;
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset, elem.mSize, data );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

}
