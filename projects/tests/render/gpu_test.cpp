// The GPU on the machine's own adapter, or its software one where there is
// none - a CI runner's. Not on the web: node has no WebGPU, and a browser
// answers only between frames.
#ifndef __EMSCRIPTEN__
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
Scope<Gpu> StartGpu()
{
    auto gpuMaybe = Gpu::TryStartNow( {} );
    if ( not gpuMaybe )
        gpuMaybe = Gpu::TryStartNow( {}, GpuOptions{ .mFallbackAdapter = true } );
    REQUIRE_MESSAGE( gpuMaybe, gpuMaybe.error() );
    return std::move( *gpuMaybe );
}
}

TEST_CASE( "A GPU clears a texture to a color picked in sRGB and reads its pixels back" )
{
    const Scope<Gpu> gpu = StartGpu();
    CHECK_FALSE( gpu->Describe().empty() );
    CHECK_FALSE( gpu->TrySurface() );

    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    descriptor.size = WGPUExtent3D{ 4, 3, 1 };
    descriptor.format = WGPUTextureFormat_RGBA8UnormSrgb;
    const GpuTexture texture( wgpuDeviceCreateTexture( gpu->Device(), &descriptor ) );
    const GpuTextureView view( wgpuTextureCreateView( texture.Get(), nullptr ) );
    // Half green as picked is a fifth in linear light, and half again when
    // stored as sRGB.
    gpu->Clear( view.Get(), Color{ .mR = 1.0f, .mG = 0.5f, .mB = 0.0f, .mA = 1.0f } );

    const auto pixelsMaybe = gpu->TryReadRgba8( texture.Get(), { 4, 3 } );
    REQUIRE_MESSAGE( pixelsMaybe, pixelsMaybe.error() );
    REQUIRE( pixelsMaybe->size() == 4 * 3 * 4 );
    for ( size_t pixel = 0; pixel < 4 * 3; ++pixel )
    {
        const u8* rgba = pixelsMaybe->data() + pixel * 4;
        CHECK( rgba[0] == 255 );
        CHECK( abs( rgba[1] - 128 ) <= 1 );
        CHECK( rgba[2] == 0 );
        CHECK( rgba[3] == 255 );
    }
}

TEST_CASE( "A window with nothing to draw into stops the GPU's start, and a start answers once" )
{
    auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = "dummy", .mMouseAsTouch = false } );
    REQUIRE_MESSAGE( platformMaybe, platformMaybe.error() );
    const auto windowMaybe = ( *platformMaybe )->TryOpenWindow();
    REQUIRE_MESSAGE( windowMaybe, windowMaybe.error() );

    const Scope<GpuStartup> startup = Gpu::Start( *windowMaybe );
    const auto finishedMaybe = startup->TryFinish();
    REQUIRE( finishedMaybe );
    CHECK( finishedMaybe->error() == "the window has nothing the GPU can draw into" );
    CHECK_THROWS_AS( startup->TryFinish(), logic_error );
}
#endif
