
#include "engine/renderer/mesh.hpp"

namespace bubble
{
Mesh::Mesh( const string& name,
            BasicMaterial&& material,
            VertexData&& vertices,
            vector<u32>&& indices )
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

    size_t size = sizeof( vec3 ) * mVertices.mPositions.size() + sizeof( vec3 ) * mVertices.mNormals.size() +
                  sizeof( vec2 ) * mVertices.mTexCoords.size() + sizeof( vec3 ) * mVertices.mTangents.size() +
                  sizeof( vec3 ) * mVertices.mBitangents.size();

    vector<char> data( size );
    size_t offset = 0;

    memmove( data.data(), mVertices.mPositions.data(), sizeof(vec3) * mVertices.mPositions.size());
    offset += sizeof( vec3 ) * mVertices.mPositions.size();

    memmove( data.data() + offset, mVertices.mNormals.data(), sizeof( vec3 ) * mVertices.mNormals.size() );
    offset += sizeof( vec3 ) * mVertices.mNormals.size();

    memmove( data.data() + offset, mVertices.mTexCoords.data(), sizeof( vec2 ) * mVertices.mTexCoords.size() );
    offset += sizeof( vec2 ) * mVertices.mTexCoords.size();

    memmove( data.data() + offset, mVertices.mTangents.data(), sizeof( vec3 ) * mVertices.mTangents.size() );
    offset += sizeof( vec3 ) * mVertices.mTangents.size();

    memmove( data.data() + offset, mVertices.mBitangents.data(), sizeof( vec3 ) * mVertices.mBitangents.size() );

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