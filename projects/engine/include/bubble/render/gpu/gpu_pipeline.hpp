#pragma once
#include "bubble/render/gpu/gpu_handle.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
class Gpu;

// A WGSL module. Its mistakes are the GPU's validation errors: logged, or
// caught by Gpu::Check with their lines.
GpuShaderHandle CreateShader( const Gpu& gpu, string_view wgsl, string_view label );

// One vertex buffer as a pipeline reads it: the bytes from one vertex to the
// next, and the attributes of a vertex at their shader locations.
struct GpuVertexBufferLayout
{
    u64 mStride = 0;
    vector<WGPUVertexAttribute> mAttributes;
};

// Depth as the engine keeps it: reversed, 1 at the near plane and 0 at
// the far one - float depth is finest near 0, which is where the distance
// is - so nearer is greater. A pass clears depth to 0.
struct GpuDepthOptions
{
    WGPUTextureFormat mFormat = WGPUTextureFormat_Depth32Float;
    WGPUCompareFunction mCompare = WGPUCompareFunction_Greater;
    bool mWrite = true;
    // Added to what is written, in the format's steps and by the slope: a
    // shadow map's caster pushed away from its light.
    i32 mBias = 0;
    f32 mBiasSlopeScale = 0;
};

// What a render pipeline is made of. Everything WebGPU fixes in a pipeline
// is here; what is not here yet - blending - comes with the passes that
// need it.
struct GpuPipelineOptions
{
    string mLabel;
    WGPUShaderModule mShader = nullptr;
    string mVertexEntry = "vs_main";
    // Empty, with no color format: no fragment stage - a pass of depth alone.
    string mFragmentEntry = "fs_main";
    // The layouts of the bind groups, group 0 first. None: the ones the
    // shader implies, which fit this pipeline's bind groups alone -
    // wgpuRenderPipelineGetBindGroupLayout gives them.
    vector<WGPUBindGroupLayout> mBindGroupLayouts;
    vector<GpuVertexBufferLayout> mVertexBuffers;
    WGPUTextureFormat mColorFormat = WGPUTextureFormat_Undefined;
    opt<GpuDepthOptions> mDepth;
    WGPUPrimitiveTopology mTopology = WGPUPrimitiveTopology_TriangleList;
    // Front faces turn counter-clockwise on screen, as in glTF.
    WGPUCullMode mCullMode = WGPUCullMode_Back;
};

GpuRenderPipelineHandle CreateRenderPipeline( const Gpu& gpu, const GpuPipelineOptions& options );
}
