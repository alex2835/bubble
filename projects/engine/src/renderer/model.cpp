
#include "engine/renderer/model.hpp"
#include <glm-aabb/AABB.hpp>

namespace bubble
{
Mesh::Mesh( const string& name,
            BasicMaterial&& material,
            VertexData&& vertices,
            vector<u32>&& indices )
    : mName( name ),
      mVertices( std::move( vertices ) ),
      mIndices( std::move( indices ) ),
      mMaterial( std::move( material ) )
{
    BufferLayout layout
    {
        { "Position",  GLSLDataType::Float3, mVertices.mPositions.size()  },
        { "Normal",    GLSLDataType::Float3, mVertices.mNormals.size()    },
        { "TexCoords", GLSLDataType::Float2, mVertices.mTexCoords.size()  },
        { "Tangent",   GLSLDataType::Float3, mVertices.mTangents.size()   },
        { "Bitangent", GLSLDataType::Float3, mVertices.mBitangents.size() }
    };

    u64 size = sizeof( vec3 ) * mVertices.mPositions.size() + 
               sizeof( vec3 ) * mVertices.mNormals.size() +
               sizeof( vec2 ) * mVertices.mTexCoords.size() +
               sizeof( vec3 ) * mVertices.mTangents.size() +
               sizeof( vec3 ) * mVertices.mBitangents.size();

    vector<char> data( size );
    u64 offset = 0;

    memmove( data.data(), mVertices.mPositions.data(), sizeof( vec3 ) * mVertices.mPositions.size() );
    offset += sizeof( vec3 ) * mVertices.mPositions.size();

    memmove( data.data() + offset, mVertices.mNormals.data(), sizeof( vec3 ) * mVertices.mNormals.size() );
    offset += sizeof( vec3 ) * mVertices.mNormals.size();

    memmove( data.data() + offset, mVertices.mTexCoords.data(), sizeof( vec2 ) * mVertices.mTexCoords.size() );
    offset += sizeof( vec2 ) * mVertices.mTexCoords.size();

    memmove( data.data() + offset, mVertices.mTangents.data(), sizeof( vec3 ) * mVertices.mTangents.size() );
    offset += sizeof( vec3 ) * mVertices.mTangents.size();

    memmove( data.data() + offset, mVertices.mBitangents.data(), sizeof( vec3 ) * mVertices.mBitangents.size() );

    auto vertex_buffer = VertexBuffer( layout, data.data(), size );
    auto index_buffer = IndexBuffer( mIndices.data(), mIndices.size() );
    mVertexArray.AddVertexBuffer( std::move( vertex_buffer ) );
    mVertexArray.SetIndexBuffer( std::move( index_buffer ) );
}

void Mesh::BindVertexArray() const
{
    mVertexArray.Bind();
}

u64 Mesh::IndiciesSize() const
{
    return mIndices.size();
}


void Mesh::ApplyMaterial( const Ref<Shader>& shader ) const
{
    mMaterial.Apply( shader );
}


// Model

Model::Model()
{
}

//void Model::CreateBoundingBox()
//{
//    for( const auto& mesh : mMeshes )
//    {
//        const auto& vertices = mesh.mVertices;
//        for( i32 i = 0; i < vertices.mPositions.size(); i++ )
//        {
//            mBoundingBox.extend( vertices.mPositions[i] );
//        }
//    }
//}

}