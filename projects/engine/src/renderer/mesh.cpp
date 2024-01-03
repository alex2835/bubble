
#include "engine/renderer/mesh.hpp"

namespace bubble
{
Mesh::Mesh( const std::string& name,
            BasicMaterial&& material,
            VertexData&& vertices,
            std::vector<uint32_t>&& indices )
    : mName( name ),
      mMaterial( std::move( material ) ),
      mVertices( std::move( vertices ) ),
      mIndices( std::move( indices ) )
{
    BufferLayout layout
    {
        { "Position",  GLSLDataType::Float3, mVertices.mPositions.size()  },
        { "Normal",    GLSLDataType::Float3, mVertices.mNormals.size()    },
        { "TexCoords", GLSLDataType::Float2, mVertices.mTexCoords.size()  },
        { "Tangent",   GLSLDataType::Float3, mVertices.mTangents.size()   },
        { "Bitangent", GLSLDataType::Float3, mVertices.mBitangents.size() }
    };

    size_t size = sizeof( glm::vec3 ) * mVertices.mPositions.size() + sizeof( glm::vec3 ) * mVertices.mNormals.size() +
                  sizeof( glm::vec2 ) * mVertices.mTexCoords.size() + sizeof( glm::vec3 ) * mVertices.mTangents.size() +
                  sizeof( glm::vec3 ) * mVertices.mBitangents.size();

    std::vector<char> data( size );
    size_t offset = 0;

    memmove( data.data(), mVertices.mPositions.data(), sizeof(glm::vec3) * mVertices.mPositions.size());
    offset += sizeof( glm::vec3 ) * mVertices.mPositions.size();

    memmove( data.data() + offset, mVertices.mNormals.data(), sizeof( glm::vec3 ) * mVertices.mNormals.size() );
    offset += sizeof( glm::vec3 ) * mVertices.mNormals.size();

    memmove( data.data() + offset, mVertices.mTexCoords.data(), sizeof( glm::vec2 ) * mVertices.mTexCoords.size() );
    offset += sizeof( glm::vec2 ) * mVertices.mTexCoords.size();

    memmove( data.data() + offset, mVertices.mTangents.data(), sizeof( glm::vec3 ) * mVertices.mTangents.size() );
    offset += sizeof( glm::vec3 ) * mVertices.mTangents.size();

    memmove( data.data() + offset, mVertices.mBitangents.data(), sizeof( glm::vec3 ) * mVertices.mBitangents.size() );

    IndexBuffer  index_buffer = IndexBuffer( mIndices.data(), mIndices.size() );
    VertexBuffer vertex_buffer = VertexBuffer( layout, data.data(), size );
    mVertexArray.AddVertexBuffer( std::move( vertex_buffer ) );
    mVertexArray.SetIndexBuffer( std::move( index_buffer ) );
}

void Mesh::BindVertetxArray() const
{
    mVertexArray.Bind();
}

size_t Mesh::IndiciesSize() const
{
    return mIndices.size();
}

}