#include "engine/pch/pch.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/log/log.hpp"
#include "engine/utils/error.hpp"
#include "engine/renderer/pipeline.hpp"
#include "engine/renderer/texture.hpp"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

namespace bubble
{
namespace
{

GpuContext* sGpuContext = nullptr;

string_view ToView( WGPUStringView sv )
{
    if ( not sv.data )
        return {};
    // A WGPUStringView is not null terminated: WGPU_STRLEN means "call strlen",
    // anything else is an explicit length.
    return sv.length == WGPU_STRLEN ? string_view( sv.data )
                                    : string_view( sv.data, sv.length );
}

void OnUncapturedError( WGPUDevice const*,
                        WGPUErrorType type,
                        WGPUStringView message,
                        void*,
                        void* )
{
    LogError( "WebGPU error (type {}): {}", (i32)type, ToView( message ) );
}

void OnDeviceLost( WGPUDevice const*,
                   WGPUDeviceLostReason reason,
                   WGPUStringView message,
                   void*,
                   void* )
{
    LogError( "WebGPU device lost (reason {}): {}", (i32)reason, ToView( message ) );
}

}


GpuContext& Gpu()
{
    BUBBLE_ASSERT( sGpuContext, "GPU context used before the window created it, or after it was destroyed" );
    return *sGpuContext;
}

void SetGpuContext( GpuContext* context )
{
    sGpuContext = context;
}


GpuContext::GpuContext( GLFWwindow* window )
{
    mInstance = wgpu::raii::Instance( wgpu::createInstance() );
    if ( not mInstance )
        throw std::runtime_error( "Could not create WebGPU instance" );

    mSurface = wgpu::raii::Surface( glfwCreateWindowWGPUSurface( *mInstance, window ) );
    if ( not mSurface )
        throw std::runtime_error( "Could not create WebGPU surface for the window" );

    wgpu::RequestAdapterOptions adapterOptions = wgpu::Default;
    adapterOptions.compatibleSurface = *mSurface;
    adapterOptions.powerPreference = wgpu::PowerPreference::HighPerformance;

    mAdapter = wgpu::raii::Adapter( mInstance->requestAdapter( adapterOptions ) );
    if ( not mAdapter )
        throw std::runtime_error( "Could not request WebGPU adapter" );

    LogAdapterInfo();

    // Callbacks are registered through the descriptor; there is no
    // setUncapturedErrorCallback method any more.
    wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
    deviceDesc.label = wgpu::StringView( "Bubble Device" );
    deviceDesc.defaultQueue.label = wgpu::StringView( "Bubble Queue" );
    deviceDesc.uncapturedErrorCallbackInfo.callback = &OnUncapturedError;
    deviceDesc.deviceLostCallbackInfo.callback = &OnDeviceLost;

    mDevice = wgpu::raii::Device( mAdapter->requestDevice( deviceDesc ) );
    if ( not mDevice )
        throw std::runtime_error( "Could not request WebGPU device" );

    mQueue = wgpu::raii::Queue( mDevice->getQueue() );
    if ( not mQueue )
        throw std::runtime_error( "Could not get WebGPU queue" );

    wgpu::SurfaceCapabilities capabilities = wgpu::Default;
    if ( mSurface->getCapabilities( *mAdapter, &capabilities ) != wgpu::Status::Success or
         capabilities.formatCount == 0 )
        throw std::runtime_error( "Could not query surface capabilities" );

    mSurfaceFormat = (wgpu::TextureFormat)capabilities.formats[0];
    capabilities.freeMembers();

    i32 width = 0;
    i32 height = 0;
    glfwGetFramebufferSize( window, &width, &height );
    ConfigureSurface( uvec2( (u32)width, (u32)height ) );
}

GpuContext::~GpuContext()
{
    if ( mConfigured )
        mSurface->unconfigure();
}

void GpuContext::LogAdapterInfo() const
{
    wgpu::AdapterInfo info = wgpu::Default;
    if ( mAdapter->getInfo( &info ) == wgpu::Status::Success )
    {
        LogInfo( "GPU: {} ({}), backend {}",
                 ToView( info.device ), ToView( info.description ), (i32)info.backendType );
        info.freeMembers();
    }

    wgpu::Limits limits = wgpu::Default;
    if ( mAdapter->getLimits( &limits ) == wgpu::Status::Success )
    {
        LogInfo( "  maxUniformBufferBindingSize={} minUniformBufferOffsetAlignment={} "
                 "maxVertexAttributes={} maxBindGroups={}",
                 limits.maxUniformBufferBindingSize,
                 limits.minUniformBufferOffsetAlignment,
                 limits.maxVertexAttributes,
                 limits.maxBindGroups );
    }
}

void GpuContext::ConfigureSurface( uvec2 size )
{
    mSize = size;

    if ( size.x == 0 or size.y == 0 )
    {
        if ( mConfigured )
        {
            mSurface->unconfigure();
            mConfigured = false;
        }
        return;
    }

    wgpu::SurfaceConfiguration config = wgpu::Default;
    config.device = *mDevice;
    config.format = mSurfaceFormat;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = size.x;
    config.height = size.y;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;
    config.presentMode = mPresentMode;

    mSurface->configure( config );
    mConfigured = true;
}

void GpuContext::SetPresentMode( wgpu::PresentMode mode )
{
    if ( mode == mPresentMode )
        return;

    // Fifo is the only mode guaranteed to exist. Anything else has to be
    // checked, or configure() fails validation.
    if ( mode != wgpu::PresentMode::Fifo )
    {
        wgpu::SurfaceCapabilities capabilities = wgpu::Default;
        if ( mSurface->getCapabilities( *mAdapter, &capabilities ) != wgpu::Status::Success )
            return;

        bool supported = false;
        for ( size_t i = 0; i < capabilities.presentModeCount; i++ )
            supported |= capabilities.presentModes[i] == (WGPUPresentMode)mode;
        capabilities.freeMembers();

        if ( not supported )
        {
            LogWarning( "Present mode {} is not supported by this surface, keeping the current one",
                        (i32)mode );
            return;
        }
    }

    mPresentMode = mode;
    if ( mConfigured )
        ConfigureSurface( mSize );
}

wgpu::raii::TextureView GpuContext::AcquireSurfaceView()
{
    if ( not mConfigured )
        return {};

    wgpu::SurfaceTexture surfaceTexture = wgpu::Default;
    mSurface->getCurrentTexture( &surfaceTexture );

    switch ( surfaceTexture.status )
    {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            // Suboptimal is still usable - present this frame and let the next
            // resize pick up a better configuration.
            break;

        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            // Recoverable: reconfigure and skip the frame rather than rendering
            // into a texture that is about to be thrown away.
            if ( surfaceTexture.texture )
                wgpuTextureRelease( surfaceTexture.texture );
            ConfigureSurface( mSize );
            return {};

        default:
            LogError( "Surface texture acquisition failed (status {})", (i32)surfaceTexture.status );
            return {};
    }

    // getCurrentTexture hands back an owned reference. It has to stay alive for
    // the rest of the frame: the view alone does not keep it from being
    // destroyed, and the commands using it are not submitted until later.
    mCurrentTexture = wgpu::raii::Texture( wgpu::Texture( surfaceTexture.texture ) );
    return wgpu::raii::TextureView( mCurrentTexture->createView() );
}

void GpuContext::Present()
{
    mSurface->present();
    // Safe to drop only once the frame has been handed to the surface.
    mCurrentTexture = {};
}

void GpuContext::PollDevice()
{
    mDevice->poll( false, nullptr );
}

StandardLayouts& GpuContext::Layouts()
{
    if ( not mLayouts )
        mLayouts = std::make_unique<StandardLayouts>();
    return *mLayouts;
}

Texture2D& GpuContext::WhiteTexture()
{
    if ( not mWhiteTexture )
    {
        auto spec = Texture2DSpecification::CreateRGBA8( uvec2( 1u ) );
        mWhiteTexture = std::make_unique<Texture2D>( spec );
        const u8 pixel[4] = { 255, 255, 255, 255 };
        mWhiteTexture->SetData( pixel, sizeof( pixel ) );
    }
    return *mWhiteTexture;
}

}
