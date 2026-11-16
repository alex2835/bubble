// Bind group layouts and bind groups, an entry at a time: what the renderer
// and the materials build theirs with. The engine's own; not a header of
// its interface.
#pragma once
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_buffer.hpp"

namespace bubble
{
inline WGPUBindGroupLayoutEntry BufferEntry( u32 binding, WGPUShaderStage visibility, WGPUBufferBindingType type )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = visibility;
    entry.buffer.type = type;
    return entry;
}

inline WGPUBindGroupLayoutEntry TextureEntry( u32 binding, WGPUTextureSampleType sampleType )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.texture.sampleType = sampleType;
    entry.texture.viewDimension = WGPUTextureViewDimension_2D;
    return entry;
}

inline WGPUBindGroupLayoutEntry SamplerEntry( u32 binding, WGPUSamplerBindingType type )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.sampler.type = type;
    return entry;
}

inline GpuBindGroupLayoutHandle CreateLayout( const Gpu& gpu,
                                              string_view label,
                                              const vector<WGPUBindGroupLayoutEntry>& entries )
{
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( label );
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return GpuBindGroupLayoutHandle( wgpuDeviceCreateBindGroupLayout( gpu.Device(), &descriptor ) );
}

inline WGPUBindGroupEntry BufferBinding( u32 binding, const GpuBuffer& buffer )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.buffer = buffer.Get();
    entry.size = buffer.Size();
    return entry;
}

inline WGPUBindGroupEntry TextureBinding( u32 binding, WGPUTextureView view )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.textureView = view;
    return entry;
}

inline WGPUBindGroupEntry SamplerBinding( u32 binding, WGPUSampler sampler )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.sampler = sampler;
    return entry;
}

inline GpuBindGroupHandle CreateGroup( const Gpu& gpu,
                                       string_view label,
                                       WGPUBindGroupLayout layout,
                                       const vector<WGPUBindGroupEntry>& entries )
{
    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( label );
    descriptor.layout = layout;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return GpuBindGroupHandle( wgpuDeviceCreateBindGroup( gpu.Device(), &descriptor ) );
}
}
