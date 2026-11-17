#include "bubble/render/gpu/gpu_model.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <cstddef>

namespace bubble
{
GpuModel::GpuModel( const Gpu& gpu, const ModelAsset& model, string_view label )
    : mMin( model.mMin ),
      mMax( model.mMax )
{
    vector<ModelVertex> vertices;
    vector<ModelSkinVertex> skin;
    vector<u32> indices;
    const bool skinned = any_of( model.mMeshes.begin(), model.mMeshes.end(),
                                 []( const ModelMesh& mesh ) { return not mesh.mSkin.empty(); } );
    for ( const ModelJoint& joint : model.mJoints )
        mRestSkin.push_back( joint.mRest * joint.mInverseBind );
    for ( const ModelMesh& mesh : model.mMeshes )
    {
        GpuMesh& gpuMesh = mMeshes.emplace_back();
        gpuMesh.mMaterial = mesh.mMaterial;
        gpuMesh.mBaseVertex = static_cast<i32>( vertices.size() );
        gpuMesh.mSkinned = not mesh.mSkin.empty();
        vertices.insert( vertices.end(), mesh.mVertices.begin(), mesh.mVertices.end() );
        // Beside every vertex, the base vertex moving both alike.
        if ( skinned )
        {
            if ( gpuMesh.mSkinned )
                skin.insert( skin.end(), mesh.mSkin.begin(), mesh.mSkin.end() );
            else
                skin.resize( vertices.size() );
        }
        // Indices stay the mesh's own: the base vertex shifts them at the draw.
        for ( const ModelLod& lod : mesh.mLods )
        {
            gpuMesh.mLods.push_back( GpuLod{ .mError = lod.mError,
                                             .mFirstIndex = static_cast<u32>( indices.size() ),
                                             .mIndexCount = static_cast<u32>( lod.mIndices.size() ) } );
            indices.insert( indices.end(), lod.mIndices.begin(), lod.mIndices.end() );
        }
    }
    if ( vertices.empty() or indices.empty() )
        return;
    mVertices = GpuBuffer( gpu, WGPUBufferUsage_Vertex, AsBytes( vertices ), format( "{} vertices", label ) );
    mIndices = GpuBuffer( gpu, WGPUBufferUsage_Index, AsBytes( indices ), format( "{} indices", label ) );
    if ( skinned )
        mSkin = GpuBuffer( gpu, WGPUBufferUsage_Vertex, AsBytes( skin ), format( "{} skin", label ) );
}

void GpuModel::Bind( WGPURenderPassEncoder pass ) const
{
    if ( not mVertices )
        return;
    wgpuRenderPassEncoderSetVertexBuffer( pass, 0, mVertices.Get(), 0, WGPU_WHOLE_SIZE );
    if ( mSkin )
        wgpuRenderPassEncoderSetVertexBuffer( pass, 1, mSkin.Get(), 0, WGPU_WHOLE_SIZE );
    wgpuRenderPassEncoderSetIndexBuffer( pass, mIndices.Get(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE );
}

void GpuModel::Draw( WGPURenderPassEncoder pass, size_t mesh, size_t lod, u32 firstInstance, u32 instances ) const
{
    if ( mesh >= mMeshes.size() or lod >= mMeshes[mesh].mLods.size() )
        throw logic_error(
            format( "GpuModel::Draw of mesh {}, level {}: it has {} meshes{}", mesh, lod, mMeshes.size(),
                    mesh < mMeshes.size() ? format( ", that one {} levels", mMeshes[mesh].mLods.size() ) : ""s ) );
    const GpuMesh& gpuMesh = mMeshes[mesh];
    const GpuLod& gpuLod = gpuMesh.mLods[lod];
    if ( gpuLod.mIndexCount == 0 )
        return;
    wgpuRenderPassEncoderDrawIndexed( pass, gpuLod.mIndexCount, instances, gpuLod.mFirstIndex, gpuMesh.mBaseVertex,
                                      firstInstance );
}

GpuVertexBufferLayout ModelVertexLayout()
{
    const auto attribute = []( WGPUVertexFormat format, u64 offset, u32 location ) {
        WGPUVertexAttribute vertexAttribute = WGPU_VERTEX_ATTRIBUTE_INIT;
        vertexAttribute.format = format;
        vertexAttribute.offset = offset;
        vertexAttribute.shaderLocation = location;
        return vertexAttribute;
    };
    return GpuVertexBufferLayout{
        .mStride = sizeof( ModelVertex ),
        .mAttributes =
            vector<WGPUVertexAttribute>{
                attribute( WGPUVertexFormat_Float32x3, offsetof( ModelVertex, mPosition ), 0 ),
                attribute( WGPUVertexFormat_Float32x3, offsetof( ModelVertex, mNormal ), 1 ),
                attribute( WGPUVertexFormat_Float32x2, offsetof( ModelVertex, mUv ), 2 ),
            },
    };
}

GpuVertexBufferLayout ModelSkinLayout()
{
    static_assert( sizeof( ModelSkinVertex ) == 24 and offsetof( ModelSkinVertex, mWeights ) == 8 );
    WGPUVertexAttribute joints = WGPU_VERTEX_ATTRIBUTE_INIT;
    joints.format = WGPUVertexFormat_Uint16x4;
    joints.offset = offsetof( ModelSkinVertex, mJoints );
    joints.shaderLocation = 3;
    WGPUVertexAttribute weights = WGPU_VERTEX_ATTRIBUTE_INIT;
    weights.format = WGPUVertexFormat_Float32x4;
    weights.offset = offsetof( ModelSkinVertex, mWeights );
    weights.shaderLocation = 4;
    return GpuVertexBufferLayout{ .mStride = sizeof( ModelSkinVertex ),
                                  .mAttributes = vector<WGPUVertexAttribute>{ joints, weights } };
}
}
