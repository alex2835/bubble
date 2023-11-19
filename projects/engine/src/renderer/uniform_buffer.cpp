
#include "renderer/uniform_buffer.hpp"


namespace bubble
{
size_t Std140DataTypeSize( GLSLDataType type )
{
    switch( type )
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


size_t Std140DataTypePadding( GLSLDataType type )
{
    switch( type )
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
    if( this != &other )
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

    for( BufferElement& elemnt : mLayout )
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
// Return size in bytes
size_t UniformBuffer::GetBufferSize()
{
    return mBufferSize;
}
// Return size of elements
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
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Int );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat( const std::string& name, float data )
{
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Float );
    SetRawData( elem, &data );
}


void UniformArrayElemnt::SetFloat2( const std::string& name, const glm::vec2& data )
{
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Float2 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat3( const std::string& name, const glm::vec3& data )
{
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Float3 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetFloat4( const std::string& name, const glm::vec4& data )
{
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Float4 );
    SetRawData( elem, glm::value_ptr( data ) );
}


void UniformArrayElemnt::SetMat4( const std::string& name, const glm::mat4& data )
{
    BufferElement* elem = FindBufferElement( name, GLSLDataType::Mat4 );
    SetRawData( elem, glm::value_ptr( data ) );
}


BufferElement* UniformArrayElemnt::FindBufferElement( const std::string& name, GLSLDataType type )
{
    auto elem = std::find_if( mLayout->begin(), mLayout->end(),
                              [&name, &type]( const BufferElement& elem )
    {
        return elem.mName == name && elem.mType == type;
    } );
    BUBBLE_ASSERT( elem != mLayout->end(), "Uniform buffer element not founded" );
    return elem._Ptr;
}

void UniformArrayElemnt::SetRawData( BufferElement* elem, const void* data )
{
    size_t array_index_offset = mLayout->mStride * mArrayIndex + elem->mOffset;
    glBindBuffer( GL_UNIFORM_BUFFER, mRendererID );
    glBufferSubData( GL_UNIFORM_BUFFER, array_index_offset, elem->mSize, data );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

}