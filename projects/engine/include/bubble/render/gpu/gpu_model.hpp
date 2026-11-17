#pragma once
#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/types/math.hpp"
#include "bubble/types/matrix.hpp"

namespace bubble
{
struct ModelAsset;

// One level of detail of a mesh: a range of its model's index buffer.
struct GpuLod
{
    f32 mError = 0;
    u32 mFirstIndex = 0;
    u32 mIndexCount = 0;
};

// One mesh of a model: where its vertices start in the model's vertex
// buffer, its levels, the full mesh first, and whether it is skinned.
struct GpuMesh
{
    string mMaterial;
    i32 mBaseVertex = 0;
    vector<GpuLod> mLods;
    bool mSkinned = false;
};

// A model on the GPU: the vertices of all its meshes in one buffer, the
// indices of all their levels in another, so the model binds once for
// every draw of it. A model with a skin has its joints and weights in a
// buffer beside the vertices, one for each of them - nothing in it for a
// mesh not skinned -, and its rest pose's skin matrices.
class GpuModel
{
public:
    GpuModel( const Gpu& gpu, const ModelAsset& model, string_view label );

    const vector<GpuMesh>& Meshes() const { return mMeshes; }
    vec3 Min() const { return mMin; }
    vec3 Max() const { return mMax; }
    // Each joint's rest times its inverse bind: what the skin is drawn with
    // when nothing poses it. Empty for a model without a skin.
    const vector<mat4>& RestSkin() const { return mRestSkin; }

    // Sets its buffers on the pass, before its draws.
    void Bind( WGPURenderPassEncoder pass ) const;
    // One level of one mesh, after Bind: `instances` of it from
    // `firstInstance` on - the shader's instance_index, which picks what it
    // draws from a storage buffer. A mesh or a level it does not have is a
    // logic_error.
    void Draw( WGPURenderPassEncoder pass, size_t mesh, size_t lod, u32 firstInstance = 0, u32 instances = 1 ) const;

private:
    // Empty for a model with nothing to draw.
    GpuBuffer mVertices;
    GpuBuffer mIndices;
    // Empty for a model with no skinned mesh.
    GpuBuffer mSkin;
    vector<GpuMesh> mMeshes;
    vector<mat4> mRestSkin;
    vec3 mMin{ 0.0f };
    vec3 mMax{ 0.0f };
};

// How a ModelVertex lies in the vertex buffer: its position at location 0,
// its normal at 1, its uv at 2.
GpuVertexBufferLayout ModelVertexLayout();
// How a ModelSkinVertex lies in the skin buffer, the second: its joints at
// location 3, its weights at 4.
GpuVertexBufferLayout ModelSkinLayout();
}
