#include "engine/pch/pch.hpp"
#include "engine/renderer/entity_id_picker.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/log/log.hpp"

namespace bubble
{
namespace
{

// copyTextureToBuffer requires the row pitch to be a multiple of 256, so the
// readback buffer is usually wider than the rectangle asked for and the rows
// have to be unpacked afterwards.
constexpr u64 cBytesPerRowAlignment = 256;

u64 AlignTo( u64 value, u64 alignment )
{
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

// What a map callback needs to find its way back. Owned by the callback, since
// the picker may be cancelled or destroyed before the map completes.
struct MapRequest
{
    EntityIdPicker* mPicker = nullptr;
    u64 mGeneration = 0;
    bool* mDone = nullptr;
};

}


void EntityIdPicker::Request( uvec2 start, uvec2 end )
{
    // A newer click supersedes whatever was in flight.
    Cancel();

    mStart = glm::min( start, end );
    mEnd = glm::max( start, end );
    mSize = uvec2( mEnd.x - mStart.x + 1, mEnd.y - mStart.y + 1 );
    mState = State::Requested;
}

void EntityIdPicker::Cancel()
{
    mState = State::Idle;
    mPixels.clear();
    mGeneration++;
}

void EntityIdPicker::CaptureFrom( Framebuffer& framebuffer )
{
    if ( mState != State::Requested )
        return;

    const Texture2D& attachment = framebuffer.ColorAttachment();
    const uvec2 attachmentSize( (u32)attachment.Width(), (u32)attachment.Height() );

    // The click may have been captured against a viewport that has since been
    // resized, which would make this copy read outside the texture.
    if ( mStart.x >= attachmentSize.x or mStart.y >= attachmentSize.y )
    {
        Cancel();
        return;
    }
    const uvec2 clampedEnd = glm::min( mEnd, attachmentSize - uvec2( 1u ) );
    mSize = uvec2( clampedEnd.x - mStart.x + 1, clampedEnd.y - mStart.y + 1 );

    mBytesPerRow = AlignTo( (u64)mSize.x * sizeof( u32 ), cBytesPerRowAlignment );
    mBufferSize = mBytesPerRow * mSize.y;

    wgpu::BufferDescriptor bufferDesc = wgpu::Default;
    bufferDesc.label = wgpu::StringView( "Entity Id Readback" );
    bufferDesc.size = mBufferSize;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    bufferDesc.mappedAtCreation = false;
    mReadback = wgpu::raii::Buffer( Gpu().Device().createBuffer( bufferDesc ) );

    wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
    encoderDesc.label = wgpu::StringView( "Entity Id Readback" );
    wgpu::raii::CommandEncoder encoder( Gpu().Device().createCommandEncoder( encoderDesc ) );

    wgpu::TexelCopyTextureInfo source = wgpu::Default;
    source.texture = attachment.GetTexture();
    source.mipLevel = 0;
    source.origin = { mStart.x, mStart.y, 0 };
    source.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferInfo destination = wgpu::Default;
    destination.buffer = *mReadback;
    destination.layout.offset = 0;
    destination.layout.bytesPerRow = (u32)mBytesPerRow;
    destination.layout.rowsPerImage = mSize.y;

    wgpu::Extent3D extent = { mSize.x, mSize.y, 1 };
    encoder->copyTextureToBuffer( source, destination, extent );

    wgpu::CommandBufferDescriptor cmdDesc = wgpu::Default;
    cmdDesc.label = wgpu::StringView( "Entity Id Readback" );
    wgpu::raii::CommandBuffer commands( encoder->finish( cmdDesc ) );
    wgpu::CommandBuffer raw = *commands;
    Gpu().Queue().submit( 1, &raw );

    mState = State::InFlight;

    auto* request = new MapRequest{ this, mGeneration, nullptr };

    wgpu::BufferMapCallbackInfo callbackInfo = wgpu::Default;
    callbackInfo.mode = wgpu::CallbackMode::AllowProcessEvents;
    callbackInfo.callback = []( WGPUMapAsyncStatus status,
                                WGPUStringView message,
                                void* userdata1,
                                void* )
    {
        Scope<MapRequest> req( (MapRequest*)userdata1 );
        EntityIdPicker* picker = req->mPicker;
        // Cancelled, or superseded by a newer click, while this was in flight.
        if ( not picker or picker->mGeneration != req->mGeneration )
            return;

        if ( status != WGPUMapAsyncStatus_Success )
        {
            LogError( "Entity id readback failed: {}",
                      message.data ? string_view( message.data,
                                                  message.length == WGPU_STRLEN
                                                      ? std::strlen( message.data )
                                                      : message.length )
                                   : string_view( "unknown" ) );
            picker->mState = State::Idle;
            return;
        }
        picker->OnMapped();
    };
    callbackInfo.userdata1 = request;

    mReadback->mapAsync( wgpu::MapMode::Read, 0, mBufferSize, callbackInfo );
}

void EntityIdPicker::OnMapped()
{
    const void* mapped = mReadback->getConstMappedRange( 0, mBufferSize );
    if ( not mapped )
    {
        mState = State::Idle;
        return;
    }

    // Unpack the padded rows into a tight rectangle.
    mPixels.resize( (u64)mSize.x * mSize.y );
    const u8* bytes = (const u8*)mapped;
    for ( u32 row = 0; row < mSize.y; row++ )
    {
        std::memcpy( mPixels.data() + (u64)row * mSize.x,
                     bytes + (u64)row * mBytesPerRow,
                     (u64)mSize.x * sizeof( u32 ) );
    }

    mReadback->unmap();
    mState = State::Ready;
}

bool EntityIdPicker::TakeResult( vector<u32>& outPixels, uvec2& outSize )
{
    if ( mState != State::Ready )
        return false;

    outPixels = std::move( mPixels );
    outSize = mSize;
    mPixels.clear();
    mState = State::Idle;
    return true;
}

}
