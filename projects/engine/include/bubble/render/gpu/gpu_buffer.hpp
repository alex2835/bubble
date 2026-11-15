#pragma once
#include "bubble/render/gpu/gpu_handle.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/number.hpp"

namespace bubble
{
class Gpu;

// A buffer on the GPU: vertices, indices, uniforms. Its size is fixed when
// it is made; every buffer can be written through the queue.
class GpuBuffer
{
public:
    GpuBuffer() = default;
    // Room for `size` bytes, written later.
    GpuBuffer( const Gpu& gpu, WGPUBufferUsage usage, u64 size, string_view label );
    // Made with `bytes` in it.
    GpuBuffer( const Gpu& gpu, WGPUBufferUsage usage, span<const byte> bytes, string_view label );

    // `bytes` at `offset`, in queue order: commands submitted before see
    // what was there. Offset and size go in 4s - the size may end at the
    // buffer's end instead; anything else is a logic_error.
    void Write( const Gpu& gpu, u64 offset, span<const byte> bytes ) const;

    WGPUBuffer Get() const { return mBuffer.Get(); }
    // As asked for; the GPU's own size is rounded up to 4.
    u64 Size() const { return mSize; }
    explicit operator bool() const { return static_cast<bool>( mBuffer ); }

private:
    GpuBufferHandle mBuffer;
    u64 mSize = 0;
};
}
