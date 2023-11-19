#pragma once
#include "utils/imexp.hpp"
#include "renderer/buffer.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT VertexArray
{
public:
    VertexArray() noexcept;
    ~VertexArray();

    VertexArray( const VertexArray& ) = delete;
    VertexArray& operator=( const VertexArray& ) = delete;

    VertexArray( VertexArray&& ) noexcept;
    VertexArray& operator=( VertexArray&& ) noexcept;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer( VertexBuffer&& vertexBuffer );
    void SetIndexBuffer( IndexBuffer&& indexBuffer );

    uint32_t GetRendererID() const;
    std::vector<VertexBuffer>& GetVertexBuffers();
    IndexBuffer& GetIndexBuffer();
    void VertexBufferIndex( uint32_t val );

private:
    GLuint mRendererID = 0;
    GLuint mVertexBufferIndex = 0;
    std::vector<VertexBuffer> mVertexBuffers;
    IndexBuffer mIndexBuffer;
};

}
