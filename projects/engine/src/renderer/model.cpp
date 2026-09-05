#include "engine/pch/pch.hpp"
#include "engine/renderer/model.hpp"
#include "engine/utils/geometry.hpp"
#include <glm-aabb/AABB.hpp>

namespace bubble
{

Mesh::Mesh( string name,
            BasicMaterial material,
            VertexBufferData vertices,
            vector<u32> indices,
            BufferType type )
    : mName( std::move( name ) ),
      mVertices( std::move( vertices ) ),
      mIndices( std::move( indices ) ),
      mMaterial( std::move( material ) )
{
    mVertexArray.SetBufferData( mVertices, mIndices, type );
}

void Mesh::BindVertexArray( wgpu::RenderPassEncoder pass ) const
{
    mVertexArray.Bind( pass );
}

u64 Mesh::IndiciesSize() const
{
    return mIndices.size();
}

void Mesh::UpdateDynamicVertexBufferData( VertexBufferData vertices, vector<u32> indices )
{
    mVertices = std::move( vertices );
    mIndices = std::move( indices );
    mVertexArray.SetBufferData( mVertices, mIndices, BufferType::Dynamic );
}

void Mesh::UpdateDynamicVertexBufferData( VertexBufferData /*vertices*/ )
{
    //BUBBLE_ASSERT( mVertexArray.GetVertexBuffer().mType == BufferType::Dynamic, "Dynamic update of static buffer" );
    //mVertices = std::move( vertices );
    //mVertexArray.GetVertexBuffer().SetData( mVertices );
}

void Mesh::ApplyMaterial( wgpu::RenderPassEncoder pass ) const
{
    mMaterial.Apply( pass );
}

// Model

Model::Model()
{

}

AABB Model::CreateBoundingBox( const Model& model )
{
    AABB bbox;
    for( const auto& mesh : model.mMeshes )
        for ( const auto& vert : mesh.mVertices.mPositions )
            bbox.extend( vert );
    return bbox;
}

}