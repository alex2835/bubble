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
            MeshSkin skin )
    : mName( std::move( name ) ),
      mVertices( std::move( vertices ) ),
      mIndices( std::move( indices ) ),
      mMaterial( std::move( material ) ),
      mSkin( std::move( skin ) )
{
    mBuffers.SetBufferData( mVertices, mIndices );
}

void Mesh::BindBuffers( wgpu::RenderPassEncoder pass ) const
{
    mBuffers.Bind( pass );
}

u64 Mesh::IndiciesSize() const
{
    return mIndices.size();
}

void Mesh::UpdateDynamicVertexBufferData( VertexBufferData vertices, vector<u32> indices )
{
    mVertices = std::move( vertices );
    mIndices = std::move( indices );
    mBuffers.SetBufferData( mVertices, mIndices );
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

const Ref<AnimationClip>& Model::FindClip( string_view name ) const
{
    static const Ref<AnimationClip> none;
    for ( const auto& clip : mClips )
        if ( clip->mName == name )
            return clip;
    return none;
}

}