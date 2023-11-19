
#include "renderer/vertex_array.hpp"

namespace bubble
{
static GLenum GLSLDataTypeToOpenGLBasemType( GLSLDataType mType )
{
    switch( mType )
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

VertexArray::VertexArray() noexcept
{
    glcall( glGenVertexArrays( 1, &mRendererID ) );
    glcall( glBindVertexArray( mRendererID ) );
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
    if( this != &other )
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
    glDeleteVertexArrays( 1, &mRendererID );
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
    BUBBLE_ASSERT( vertexBuffer.GetLayout().GetElements().size(), "Vertex Buffer has no layout!" );

    Bind();
    vertexBuffer.Bind();

    const auto& layout = vertexBuffer.GetLayout();
    for( const auto& element : layout )
    {
        switch( element.mType )
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
            for( uint32_t i = 0; i < count; i++ )
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

std::vector<bubble::VertexBuffer>& VertexArray::GetVertexBuffers()
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

}
