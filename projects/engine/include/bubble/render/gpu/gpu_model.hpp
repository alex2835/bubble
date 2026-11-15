#pragma once
#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/types/math.hpp"

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
// buffer, and its levels, the full mesh first.
struct GpuMesh
{
    string mMaterial;
    i32 mBaseVertex = 0;
    vector<GpuLod> mLods;
};

// A model on the GPU: the vertices of all its meshes in one buffer, the
// indices of all their levels in another, so the model binds once for
// every draw of it.
class GpuModel
{
public:
    GpuModel( const Gpu& gpu, const ModelAsset& model, string_view label );

    const vector<GpuMesh>& Meshes() const { return mMeshes; }
    vec3 Min() const { return mMin; }
    vec3 Max() const { return mMax; }

    // Sets its buffers on the pass, before its draws.
    void Bind( WGPURenderPassEncoder pass ) const;
    // One level of one mesh, after Bind. A mesh or a level it does not have
    // is a logic_error.
    void Draw( WGPURenderPassEncoder pass, size_t mesh, size_t lod, u32 instances = 1 ) const;

private:
    // Empty for a model with nothing to draw.
    GpuBuffer mVertices;
    GpuBuffer mIndices;
    vector<GpuMesh> mMeshes;
    vec3 mMin{ 0.0f };
    vec3 mMax{ 0.0f };
};

// How a ModelVertex lies in the vertex buffer: its position at location 0,
// its normal at 1, its uv at 2.
GpuVertexBufferLayout ModelVertexLayout();
}
