#include "bubble/render/gpu/gpu.hpp"
#include "bubble/core/log.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"
#ifndef __EMSCRIPTEN__
#include <webgpu/wgpu.h>
#endif

namespace bubble
{
// What the system has answered so far: shared with the callbacks, which
// may come after the GpuStartup that asked has gone.
struct GpuStartup::State
{
    GpuInstance mInstance;
    GpuSurfaceHandle mSurface;
    GpuAdapter mAdapter;
    GpuDevice mDevice;
    bool mAskedForDevice = false;
    opt<string> mError;
    bool mSpent = false;
};

namespace
{
using StartState = GpuStartup::State;

string_view BackendName( WGPUBackendType backend )
{
    switch ( backend )
    {
        case WGPUBackendType_Null: return "no backend";
        case WGPUBackendType_WebGPU: return "WebGPU";
        case WGPUBackendType_D3D11: return "D3D11";
        case WGPUBackendType_D3D12: return "D3D12";
        case WGPUBackendType_Metal: return "Metal";
        case WGPUBackendType_Vulkan: return "Vulkan";
        case WGPUBackendType_OpenGL: return "OpenGL";
        case WGPUBackendType_OpenGLES: return "OpenGL ES";
        default: return "an unknown backend";
    }
}

string_view AdapterTypeName( WGPUAdapterType type )
{
    switch ( type )
    {
        case WGPUAdapterType_DiscreteGPU: return "discrete GPU";
        case WGPUAdapterType_IntegratedGPU: return "integrated GPU";
        case WGPUAdapterType_CPU: return "software";
        default: return "unknown kind";
    }
}

string DescribeAdapter( WGPUAdapter adapter )
{
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
    if ( wgpuAdapterGetInfo( adapter, &info ) != WGPUStatus_Success )
        return "an adapter that does not say what it is";
    string description = format( "{}, {}, {}", ToStringView( info.device ), BackendName( info.backendType ),
                                 AdapterTypeName( info.adapterType ) );
    wgpuAdapterInfoFreeMembers( info );
    return description;
}

// The callbacks get the state through userdata1: a Ref of their own, taken
// back - and dropped - when they are called.
void* Hold( const Ref<StartState>& state )
{
    return CreateScope<Ref<StartState>>( state ).release();
}

Scope<Ref<StartState>> TakeBack( void* held )
{
    return Scope<Ref<StartState>>( static_cast<Ref<StartState>*>( held ) );
}

void OnAdapter( WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* held, void* )
{
    StartState& state = **TakeBack( held );
    if ( status == WGPURequestAdapterStatus_Success )
        state.mAdapter = GpuAdapter( adapter );
    else
        state.mError = format( "no GPU adapter: {}", ToStringView( message ) );
}

void OnDevice( WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* held, void* )
{
    StartState& state = **TakeBack( held );
    if ( status == WGPURequestDeviceStatus_Success )
        state.mDevice = GpuDevice( device );
    else
        state.mError = format( "no GPU device: {}", ToStringView( message ) );
}

void OnUncapturedError( WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void* )
{
    LogError( "GPU error {}: {}", static_cast<i32>( type ), ToStringView( message ) );
}

void OnDeviceLost( WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void*, void* )
{
    // Released by us, or never made: nothing was lost.
    if ( reason == WGPUDeviceLostReason_Destroyed or reason == WGPUDeviceLostReason_CallbackCancelled )
        return;
    LogError( "GPU device lost: {}", ToStringView( message ) );
}

expected<GpuSurfaceHandle, string> TryCreateSurface( WGPUInstance instance, const NativeWindow& native )
{
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    // Each source lives until the surface is made from it.
    WGPUSurfaceSourceWindowsHWND win32 = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
    WGPUSurfaceSourceXlibWindow x11 = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
    WGPUSurfaceSourceWaylandSurface wayland = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
#ifdef __EMSCRIPTEN__
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
#endif
    if ( const auto* window = get_if<Win32Window>( &native ) )
    {
        win32.hinstance = window->mInstance;
        win32.hwnd = window->mWindow;
        descriptor.nextInChain = &win32.chain;
    }
    else if ( const auto* window = get_if<X11Window>( &native ) )
    {
        x11.display = window->mDisplay;
        x11.window = window->mWindow;
        descriptor.nextInChain = &x11.chain;
    }
    else if ( const auto* window = get_if<WaylandWindow>( &native ) )
    {
        wayland.display = window->mDisplay;
        wayland.surface = window->mSurface;
        descriptor.nextInChain = &wayland.chain;
    }
#ifdef __EMSCRIPTEN__
    else if ( const auto* window = get_if<CanvasWindow>( &native ) )
    {
        canvas.selector = ToGpuString( window->mSelector );
        descriptor.nextInChain = &canvas.chain;
    }
#endif
    else
        return unexpected( "the window has nothing the GPU can draw into"s );
    GpuSurfaceHandle surface( wgpuInstanceCreateSurface( instance, &descriptor ) );
    if ( not surface )
        return unexpected( "the GPU would not draw into the window"s );
    return surface;
}

// A surface's texture as it is, and the format its views have: sRGB
// whenever there is one, so what is drawn in linear light is stored as a
// screen shows it.
WGPUTextureFormat SrgbOf( WGPUTextureFormat format )
{
    switch ( format )
    {
        case WGPUTextureFormat_BGRA8Unorm: return WGPUTextureFormat_BGRA8UnormSrgb;
        case WGPUTextureFormat_RGBA8Unorm: return WGPUTextureFormat_RGBA8UnormSrgb;
        default: return format;
    }
}

// sRGB as the artist picks it to linear light, which the GPU works in.
f64 Linear( f32 srgb )
{
    return srgb <= 0.04045f ? srgb / 12.92 : pow( ( srgb + 0.055 ) / 1.055, 2.4 );
}
}

GpuSurface::GpuSurface( GpuSurfaceHandle surface, WGPUDevice device, WGPUTextureFormat format )
    : mSurface( std::move( surface ) ),
      mDevice( device ),
      mFormat( format )
{
}

GpuSurface::~GpuSurface()
{
    if ( mConfigured and mSurface )
        wgpuSurfaceUnconfigure( mSurface.Get() );
}

void GpuSurface::Resize( ivec2 pixels )
{
    if ( pixels == mSize )
        return;
    mSize = pixels;
    Configure();
}

void GpuSurface::Configure()
{
    if ( mSize.x <= 0 or mSize.y <= 0 )
    {
        if ( mConfigured )
            wgpuSurfaceUnconfigure( mSurface.Get() );
        mConfigured = false;
        return;
    }
    const WGPUTextureFormat viewFormat = SrgbOf( mFormat );
    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = mDevice;
    configuration.format = mFormat;
    configuration.usage = WGPUTextureUsage_RenderAttachment;
    configuration.width = static_cast<u32>( mSize.x );
    configuration.height = static_cast<u32>( mSize.y );
    configuration.viewFormatCount = viewFormat == mFormat ? 0 : 1;
    configuration.viewFormats = &viewFormat;
    configuration.alphaMode = WGPUCompositeAlphaMode_Auto;
    // What the display shows, when it shows it: the frame waits for it.
    configuration.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure( mSurface.Get(), &configuration );
    mConfigured = true;
}

opt<GpuTextureView> GpuSurface::TryAcquire()
{
    if ( not mConfigured )
        return nullopt;
    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture( mSurface.Get(), &surfaceTexture );
    GpuTexture texture( surfaceTexture.texture );
    switch ( surfaceTexture.status )
    {
        // Suboptimal still shows; the next resize configures it better.
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal: break;
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost: Configure(); return nullopt;
        default: return nullopt;
    }
    mCurrent = std::move( texture );
    WGPUTextureViewDescriptor view = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    view.format = SrgbOf( mFormat );
    return GpuTextureView( wgpuTextureCreateView( mCurrent.Get(), &view ) );
}

void GpuSurface::Present()
{
    if ( not mCurrent )
        return;
    // A browser shows the canvas itself when the frame ends.
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent( mSurface.Get() );
#endif
    mCurrent.Reset();
}

GpuStartup::~GpuStartup() = default;

opt<expected<Scope<Gpu>, string>> GpuStartup::TryFinish()
{
    State& state = *mState;
    if ( state.mSpent )
        throw logic_error( "GpuStartup::TryFinish: it gave its Gpu or its error already" );
    if ( state.mInstance and not state.mError )
        wgpuInstanceProcessEvents( state.mInstance.Get() );
    if ( state.mError )
    {
        state.mSpent = true;
        return expected<Scope<Gpu>, string>( unexpected( *state.mError ) );
    }
    if ( state.mAdapter and not state.mAskedForDevice )
    {
        state.mAskedForDevice = true;
        WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "bubble" );
        descriptor.uncapturedErrorCallbackInfo.callback = &OnUncapturedError;
        descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
        descriptor.deviceLostCallbackInfo.callback = &OnDeviceLost;
        WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowProcessEvents;
        callback.callback = &OnDevice;
        callback.userdata1 = Hold( mState );
        wgpuAdapterRequestDevice( state.mAdapter.Get(), &descriptor, callback );
        return nullopt;
    }
    if ( not state.mDevice )
        return nullopt;

    Scope<Gpu> gpu( new Gpu() );
    gpu->mDescription = DescribeAdapter( state.mAdapter.Get() );
    gpu->mQueue = GpuQueue( wgpuDeviceGetQueue( state.mDevice.Get() ) );
    if ( state.mSurface )
    {
        WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
        if ( wgpuSurfaceGetCapabilities( state.mSurface.Get(), state.mAdapter.Get(), &capabilities ) !=
                 WGPUStatus_Success or
             capabilities.formatCount == 0 )
        {
            state.mSpent = true;
            return expected<Scope<Gpu>, string>( unexpected( "the GPU names no format to draw the window in"s ) );
        }
        const WGPUTextureFormat format = capabilities.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers( capabilities );
        gpu->mSurface = GpuSurface( std::move( state.mSurface ), state.mDevice.Get(), format );
    }
    gpu->mInstance = std::move( state.mInstance );
    gpu->mAdapter = std::move( state.mAdapter );
    gpu->mDevice = std::move( state.mDevice );
    state.mSpent = true;
    LogInfo( "GPU: {}", gpu->mDescription );
    return expected<Scope<Gpu>, string>( std::move( gpu ) );
}

Scope<GpuStartup> Gpu::Start( OptRef<const Window> window, GpuOptions options )
{
    const auto state = CreateRef<StartState>();
    Scope<GpuStartup> startup( new GpuStartup( state ) );
    state->mInstance = GpuInstance( wgpuCreateInstance( nullptr ) );
    if ( not state->mInstance )
    {
        state->mError = "no WebGPU here";
        return startup;
    }
    if ( window )
    {
        auto surfaceMaybe = TryCreateSurface( state->mInstance.Get(), window->Native() );
        if ( not surfaceMaybe )
        {
            state->mError = std::move( surfaceMaybe.error() );
            return startup;
        }
        state->mSurface = std::move( *surfaceMaybe );
    }
    WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
    adapterOptions.forceFallbackAdapter = options.mFallbackAdapter;
    adapterOptions.compatibleSurface = state->mSurface.Get();
    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    // Answers come only inside wgpuInstanceProcessEvents - in TryFinish, on
    // the thread that asked.
    callback.mode = WGPUCallbackMode_AllowProcessEvents;
    callback.callback = &OnAdapter;
    callback.userdata1 = Hold( state );
    wgpuInstanceRequestAdapter( state->mInstance.Get(), &adapterOptions, callback );
    return startup;
}

#ifndef __EMSCRIPTEN__
expected<Scope<Gpu>, string> Gpu::TryStartNow( OptRef<const Window> window, GpuOptions options )
{
    const Scope<GpuStartup> startup = Start( window, options );
    for ( u32 waited = 0; waited < 10'000; ++waited )
    {
        if ( auto finishedMaybe = startup->TryFinish() )
            return std::move( *finishedMaybe );
        this_thread::sleep_for( chrono::milliseconds( 1 ) );
    }
    return unexpected( "the GPU did not answer in ten seconds"s );
}
#endif

Gpu::~Gpu() = default;

OptRef<GpuSurface> Gpu::TrySurface()
{
    if ( not mSurface )
        return {};
    return *mSurface;
}

void Gpu::Clear( WGPUTextureView target, Color color )
{
    const GpuCommandEncoder encoder( wgpuDeviceCreateCommandEncoder( mDevice.Get(), nullptr ) );
    WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    attachment.view = target;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = WGPUColor{ Linear( color.mR ), Linear( color.mG ), Linear( color.mB ), color.mA };
    WGPURenderPassDescriptor pass = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    pass.colorAttachmentCount = 1;
    pass.colorAttachments = &attachment;
    const GpuRenderPass passEncoder( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &pass ) );
    wgpuRenderPassEncoderEnd( passEncoder.Get() );
    const GpuCommandBuffer commands( wgpuCommandEncoderFinish( encoder.Get(), nullptr ) );
    const WGPUCommandBuffer submitted = commands.Get();
    wgpuQueueSubmit( mQueue.Get(), 1, &submitted );
}

void Gpu::Poll()
{
#ifndef __EMSCRIPTEN__
    wgpuDevicePoll( mDevice.Get(), false, nullptr );
#endif
    wgpuInstanceProcessEvents( mInstance.Get() );
}

#ifndef __EMSCRIPTEN__
expected<vector<u8>, string> Gpu::TryReadRgba8( WGPUTexture texture, ivec2 size )
{
    // A copy's rows are 256 bytes apart at least.
    const u32 rowBytes = static_cast<u32>( size.x ) * 4;
    const u32 paddedRowBytes = ( rowBytes + 255 ) / 256 * 256;
    const u64 bufferBytes = u64{ paddedRowBytes } * static_cast<u32>( size.y );
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bufferDescriptor.size = bufferBytes;
    const GpuBuffer buffer( wgpuDeviceCreateBuffer( mDevice.Get(), &bufferDescriptor ) );

    const GpuCommandEncoder encoder( wgpuDeviceCreateCommandEncoder( mDevice.Get(), nullptr ) );
    WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    source.texture = texture;
    WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    destination.buffer = buffer.Get();
    destination.layout.bytesPerRow = paddedRowBytes;
    destination.layout.rowsPerImage = static_cast<u32>( size.y );
    const WGPUExtent3D extent{ static_cast<u32>( size.x ), static_cast<u32>( size.y ), 1 };
    wgpuCommandEncoderCopyTextureToBuffer( encoder.Get(), &source, &destination, &extent );
    const GpuCommandBuffer commands( wgpuCommandEncoderFinish( encoder.Get(), nullptr ) );
    const WGPUCommandBuffer submitted = commands.Get();
    wgpuQueueSubmit( mQueue.Get(), 1, &submitted );

    struct Mapping
    {
        bool mDone = false;
        WGPUMapAsyncStatus mStatus = WGPUMapAsyncStatus_Error;
        string mMessage;
    } mapping;
    WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowProcessEvents;
    // Waited for below, so the mapping on this stack outlives the call.
    callback.callback = []( WGPUMapAsyncStatus status, WGPUStringView message, void* held, void* ) {
        Mapping& done = *static_cast<Mapping*>( held );
        done.mDone = true;
        done.mStatus = status;
        done.mMessage = string( ToStringView( message ) );
    };
    callback.userdata1 = &mapping;
    wgpuBufferMapAsync( buffer.Get(), WGPUMapMode_Read, 0, static_cast<size_t>( bufferBytes ), callback );
    for ( u32 polls = 0; not mapping.mDone and polls < 1000; ++polls )
    {
        wgpuDevicePoll( mDevice.Get(), true, nullptr );
        wgpuInstanceProcessEvents( mInstance.Get() );
    }
    if ( not mapping.mDone or mapping.mStatus != WGPUMapAsyncStatus_Success )
        return unexpected( format( "the pixels did not come back: {}",
                                   mapping.mDone ? mapping.mMessage : "the GPU never answered"s ) );

    const auto* mapped =
        static_cast<const u8*>( wgpuBufferGetConstMappedRange( buffer.Get(), 0, static_cast<size_t>( bufferBytes ) ) );
    vector<u8> pixels( size_t{ rowBytes } * static_cast<u32>( size.y ) );
    for ( i32 row = 0; row < size.y; ++row )
        copy_n( mapped + size_t{ paddedRowBytes } * row, rowBytes, pixels.data() + size_t{ rowBytes } * row );
    wgpuBufferUnmap( buffer.Get() );
    return pixels;
}
#endif
}
