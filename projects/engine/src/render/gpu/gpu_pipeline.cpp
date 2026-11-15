#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/gpu.hpp"

namespace bubble
{
GpuShaderHandle CreateShader( const Gpu& gpu, string_view wgsl, string_view label )
{
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = ToGpuString( wgsl );
    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &source.chain;
    descriptor.label = ToGpuString( label );
    return GpuShaderHandle( wgpuDeviceCreateShaderModule( gpu.Device(), &descriptor ) );
}

GpuRenderPipelineHandle CreateRenderPipeline( const Gpu& gpu, const GpuPipelineOptions& options )
{
    GpuPipelineLayoutHandle layout;
    if ( not options.mBindGroupLayouts.empty() )
    {
        WGPUPipelineLayoutDescriptor layoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        layoutDescriptor.label = ToGpuString( options.mLabel );
        layoutDescriptor.bindGroupLayoutCount = options.mBindGroupLayouts.size();
        layoutDescriptor.bindGroupLayouts = options.mBindGroupLayouts.data();
        layout = GpuPipelineLayoutHandle( wgpuDeviceCreatePipelineLayout( gpu.Device(), &layoutDescriptor ) );
    }

    vector<WGPUVertexBufferLayout> buffers;
    buffers.reserve( options.mVertexBuffers.size() );
    for ( const GpuVertexBufferLayout& buffer : options.mVertexBuffers )
    {
        WGPUVertexBufferLayout bufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        bufferLayout.stepMode = WGPUVertexStepMode_Vertex;
        bufferLayout.arrayStride = buffer.mStride;
        bufferLayout.attributeCount = buffer.mAttributes.size();
        bufferLayout.attributes = buffer.mAttributes.data();
        buffers.push_back( bufferLayout );
    }

    WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
    target.format = options.mColorFormat;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = options.mShader;
    fragment.entryPoint = ToGpuString( options.mFragmentEntry );
    fragment.targetCount = 1;
    fragment.targets = &target;

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( options.mLabel );
    descriptor.layout = layout.Get();
    descriptor.vertex.module = options.mShader;
    descriptor.vertex.entryPoint = ToGpuString( options.mVertexEntry );
    descriptor.vertex.bufferCount = buffers.size();
    descriptor.vertex.buffers = buffers.data();
    descriptor.primitive.topology = options.mTopology;
    descriptor.primitive.frontFace = WGPUFrontFace_CCW;
    descriptor.primitive.cullMode = options.mCullMode;
    descriptor.fragment = &fragment;
    // The pipeline keeps its layout; ours goes when this returns.
    return GpuRenderPipelineHandle( wgpuDeviceCreateRenderPipeline( gpu.Device(), &descriptor ) );
}
}
