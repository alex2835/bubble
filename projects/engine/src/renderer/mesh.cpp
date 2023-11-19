
#include "renderer/mesh.hpp"

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
        { GLSLDataType::Float3, "Position",  mVertices.mPositions.size()  },
        { GLSLDataType::Float3, "Normal",    mVertices.mNormals.size()    },
        { GLSLDataType::Float2, "TexCoords", mVertices.mTexCoords.size()  },
        { GLSLDataType::Float3, "Tangent",   mVertices.mTangents.size()   },
        { GLSLDataType::Float3, "Bitangent", mVertices.mBitangents.size() }
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
    VertexBuffer vertex_buffer = VertexBuffer( data.data(), size );
    vertex_buffer.SetLayout( layout );

    mVertexArray.AddVertexBuffer( std::move( vertex_buffer ) );
    mVertexArray.SetIndexBuffer( std::move( index_buffer ) );
}

}