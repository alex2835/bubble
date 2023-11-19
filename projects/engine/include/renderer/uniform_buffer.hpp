#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "utils/imexp.hpp"
#include "renderer/buffer.hpp"

namespace bubble
{
struct UniformArrayElemnt;
size_t Std140DataTypeSize( GLSLDataType type );
size_t Std140DataTypePadding( GLSLDataType type );


struct BUBBLE_ENGINE_EXPORT UniformBuffer
{
    GLuint mRendererID = 0;
    size_t mBufferSize = 0;
    BufferLayout mLayout;
    size_t mSize = 0; // elements in array
    size_t mIndex = 0;

    UniformBuffer() = default;
    // additional size necessary if buffer contain more then one array (for example nLights)
    UniformBuffer( int index, const BufferLayout& layout, uint32_t size = 1, uint32_t additional_size = 0 );

    UniformBuffer( const UniformBuffer& ) = delete;
    UniformBuffer& operator=( const UniformBuffer& ) = delete;

    UniformBuffer( UniformBuffer&& ) noexcept;
    UniformBuffer& operator=( UniformBuffer&& ) noexcept;

    ~UniformBuffer();

    // Raw (Don't forget to observe std140 paddings)
    void SetData( const void* data, uint32_t size, uint32_t offset = 0 );

    // Save (Use it event only one element in buffer)
    UniformArrayElemnt operator[] ( int index );

    const BufferLayout& GetLayout() const;
    // Return size in bytes
    size_t GetBufferSize();
    // Return size of elements
    size_t GetSize();

private:
    // Recalculate size and offset of elements for std140
    void CalculateOffsetsAndStride();
};


/*
    Doesn't own any resources
    Point to current element in array
*/
struct BUBBLE_ENGINE_EXPORT UniformArrayElemnt
{
    uint32_t mRendererID = 0;
    uint32_t mArrayIndex = 0;
    const BufferLayout* mLayout;

    UniformArrayElemnt( const UniformBuffer& uniform_buffer, int index );

    // Raw
    void SetData( const void* data, size_t size = 0, uint32_t offset = 0 );

    // Save
    void SetInt( const std::string& name, int   data );
    void SetFloat( const std::string& name, float data );
    void SetFloat2( const std::string& name, const glm::vec2& data );
    void SetFloat3( const std::string& name, const glm::vec3& data );
    void SetFloat4( const std::string& name, const glm::vec4& data );
    void SetMat4( const std::string& name, const glm::mat4& data );

private:
    BufferElement* FindBufferElement( const std::string& name, GLSLDataType type );
    void SetRawData( BufferElement* elem, const void* data );
};

}