#pragma once
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"
#include <webgpu/webgpu.h>

namespace bubble
{
// How each WebGPU object is let go: by overload, as each handle is a type
// of its own.
inline void GpuRelease( WGPUInstance handle )
{
    wgpuInstanceRelease( handle );
}
inline void GpuRelease( WGPUAdapter handle )
{
    wgpuAdapterRelease( handle );
}
inline void GpuRelease( WGPUDevice handle )
{
    wgpuDeviceRelease( handle );
}
inline void GpuRelease( WGPUQueue handle )
{
    wgpuQueueRelease( handle );
}
inline void GpuRelease( WGPUSurface handle )
{
    wgpuSurfaceRelease( handle );
}
inline void GpuRelease( WGPUTexture handle )
{
    wgpuTextureRelease( handle );
}
inline void GpuRelease( WGPUTextureView handle )
{
    wgpuTextureViewRelease( handle );
}
inline void GpuRelease( WGPUBuffer handle )
{
    wgpuBufferRelease( handle );
}
inline void GpuRelease( WGPUCommandEncoder handle )
{
    wgpuCommandEncoderRelease( handle );
}
inline void GpuRelease( WGPUCommandBuffer handle )
{
    wgpuCommandBufferRelease( handle );
}
inline void GpuRelease( WGPURenderPassEncoder handle )
{
    wgpuRenderPassEncoderRelease( handle );
}
inline void GpuRelease( WGPUSampler handle )
{
    wgpuSamplerRelease( handle );
}
inline void GpuRelease( WGPUShaderModule handle )
{
    wgpuShaderModuleRelease( handle );
}
inline void GpuRelease( WGPUBindGroupLayout handle )
{
    wgpuBindGroupLayoutRelease( handle );
}
inline void GpuRelease( WGPUBindGroup handle )
{
    wgpuBindGroupRelease( handle );
}
inline void GpuRelease( WGPUPipelineLayout handle )
{
    wgpuPipelineLayoutRelease( handle );
}
inline void GpuRelease( WGPURenderPipeline handle )
{
    wgpuRenderPipelineRelease( handle );
}

// One WebGPU object of the C API, owned: released when it goes, moved but
// never copied. Get() lends the handle to a call and keeps it. The aliases
// below end in Handle; the names without it are ours - GpuBuffer,
// GpuTexture - built on them.
template <typename T>
class GpuHandle
{
public:
    GpuHandle() = default;
    explicit GpuHandle( T handle ) : mHandle( handle ) {}
    ~GpuHandle() { Reset(); }
    GpuHandle( GpuHandle&& other ) noexcept : mHandle( exchange( other.mHandle, nullptr ) ) {}
    GpuHandle& operator=( GpuHandle&& other ) noexcept
    {
        if ( this != &other )
        {
            Reset();
            mHandle = exchange( other.mHandle, nullptr );
        }
        return *this;
    }
    GpuHandle( const GpuHandle& ) = delete;
    GpuHandle& operator=( const GpuHandle& ) = delete;

    T Get() const { return mHandle; }
    explicit operator bool() const { return mHandle != nullptr; }
    void Reset()
    {
        if ( mHandle != nullptr )
            GpuRelease( mHandle );
        mHandle = nullptr;
    }

private:
    T mHandle = nullptr;
};

using GpuInstanceHandle = GpuHandle<WGPUInstance>;
using GpuAdapterHandle = GpuHandle<WGPUAdapter>;
using GpuDeviceHandle = GpuHandle<WGPUDevice>;
using GpuQueueHandle = GpuHandle<WGPUQueue>;
using GpuSurfaceHandle = GpuHandle<WGPUSurface>;
using GpuTextureHandle = GpuHandle<WGPUTexture>;
using GpuTextureViewHandle = GpuHandle<WGPUTextureView>;
using GpuBufferHandle = GpuHandle<WGPUBuffer>;
using GpuCommandEncoderHandle = GpuHandle<WGPUCommandEncoder>;
using GpuCommandBufferHandle = GpuHandle<WGPUCommandBuffer>;
using GpuRenderPassHandle = GpuHandle<WGPURenderPassEncoder>;
using GpuSamplerHandle = GpuHandle<WGPUSampler>;
using GpuShaderHandle = GpuHandle<WGPUShaderModule>;
using GpuBindGroupLayoutHandle = GpuHandle<WGPUBindGroupLayout>;
using GpuBindGroupHandle = GpuHandle<WGPUBindGroup>;
using GpuPipelineLayoutHandle = GpuHandle<WGPUPipelineLayout>;
using GpuRenderPipelineHandle = GpuHandle<WGPURenderPipeline>;

// WebGPU's strings are views with a length or WGPU_STRLEN, which says "up to
// the terminating zero".
inline string_view ToStringView( WGPUStringView text )
{
    if ( text.data == nullptr )
        return {};
    return text.length == WGPU_STRLEN ? string_view( text.data ) : string_view( text.data, text.length );
}

inline WGPUStringView ToGpuString( string_view text )
{
    return WGPUStringView{ text.data(), text.size() };
}
}
