#pragma once
#include "bubble/render/gpu/gpu_handle.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"

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

// What a render pipeline is made of. Everything WebGPU fixes in a pipeline
// is here; what is not here yet - depth, blending - comes with the passes
// that need it.
struct GpuPipelineOptions
{
    string mLabel;
    WGPUShaderModule mShader = nullptr;
    string mVertexEntry = "vs_main";
    string mFragmentEntry = "fs_main";
    // The layouts of the bind groups, group 0 first. None: the ones the
    // shader implies, which fit this pipeline's bind groups alone -
    // wgpuRenderPipelineGetBindGroupLayout gives them.
    vector<WGPUBindGroupLayout> mBindGroupLayouts;
    vector<GpuVertexBufferLayout> mVertexBuffers;
    WGPUTextureFormat mColorFormat = WGPUTextureFormat_Undefined;
    WGPUPrimitiveTopology mTopology = WGPUPrimitiveTopology_TriangleList;
    // Front faces turn counter-clockwise on screen, as in glTF.
    WGPUCullMode mCullMode = WGPUCullMode_Back;
};

GpuRenderPipelineHandle CreateRenderPipeline( const Gpu& gpu, const GpuPipelineOptions& options );
}
