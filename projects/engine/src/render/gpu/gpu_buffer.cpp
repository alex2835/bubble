#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// Buffer sizes and the ranges the queue writes are multiples of 4.
u64 RoundUp4( u64 size )
{
    return ( size + 3 ) / 4 * 4;
}

GpuBufferHandle CreateBuffer( const Gpu& gpu, WGPUBufferUsage usage, u64 size, string_view label, bool mapped )
{
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( label );
    descriptor.usage = usage | WGPUBufferUsage_CopyDst;
    descriptor.size = RoundUp4( size );
    descriptor.mappedAtCreation = mapped;
    return GpuBufferHandle( wgpuDeviceCreateBuffer( gpu.Device(), &descriptor ) );
}
}

GpuBuffer::GpuBuffer( const Gpu& gpu, WGPUBufferUsage usage, u64 size, string_view label )
    : mBuffer( CreateBuffer( gpu, usage, size, label, false ) ),
      mSize( size )
{
}

GpuBuffer::GpuBuffer( const Gpu& gpu, WGPUBufferUsage usage, span<const byte> bytes, string_view label )
    : mBuffer( CreateBuffer( gpu, usage, bytes.size(), label, true ) ),
      mSize( bytes.size() )
{
    // Mapped from the start: the bytes go in here and now, of any size,
    // with no copy waiting in the queue.
    const size_t mappedSize = static_cast<size_t>( RoundUp4( mSize ) );
    auto* mapped = static_cast<byte*>( wgpuBufferGetMappedRange( mBuffer.Get(), 0, mappedSize ) );
    if ( mapped != nullptr )
        copy_n( bytes.data(), bytes.size(), mapped );
    wgpuBufferUnmap( mBuffer.Get() );
}

void GpuBuffer::Write( const Gpu& gpu, u64 offset, span<const byte> bytes ) const
{
    const u64 end = offset + bytes.size();
    if ( offset % 4 != 0 or end > mSize or ( bytes.size() % 4 != 0 and end != mSize ) )
        throw logic_error( format( "GpuBuffer::Write of {} bytes at {} into {}: past the end, or not in 4s",
                                   bytes.size(), offset, mSize ) );
    const u64 whole = bytes.size() / 4 * 4;
    if ( whole > 0 )
        wgpuQueueWriteBuffer( gpu.Queue(), mBuffer.Get(), offset, bytes.data(), static_cast<size_t>( whole ) );
    // The last few bytes at the very end, padded to 4 into the room the
    // rounded-up size leaves.
    if ( whole < bytes.size() )
    {
        array<byte, 4> tail{};
        copy_n( bytes.data() + whole, bytes.size() - whole, tail.data() );
        wgpuQueueWriteBuffer( gpu.Queue(), mBuffer.Get(), offset + whole, tail.data(), tail.size() );
    }
}
}
