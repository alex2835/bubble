// The GPU on the machine's own adapter, or its software one where there is
// none - a CI runner's. Not on the web: node has no WebGPU, and a browser
// answers only between frames.
#ifndef __EMSCRIPTEN__
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_texture.hpp"
#include "bubble/types/stream.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A GPU clears a texture to a color picked in sRGB and reads its pixels back" )
{
    const Scope<Gpu> gpu = StartGpu();
    CHECK_FALSE( gpu->Describe().empty() );
    CHECK_FALSE( gpu->TrySurface() );

    const GpuTexture texture( *gpu,
                              GpuTextureOptions{ .mSize = { 4, 3 },
                                                 .mMips = 1,
                                                 .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                                 .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                                 .mLabel = "target" } );
    // Half green as picked is a fifth in linear light, and half again when
    // stored as sRGB.
    gpu->Clear( texture.View(), Color{ .mR = 1.0f, .mG = 0.5f, .mB = 0.0f, .mA = 1.0f } );

    const vector<u8> pixels = Required( gpu->TryReadRgba8( texture ) );
    REQUIRE( pixels.size() == 4 * 3 * 4 );
    for ( size_t pixel = 0; pixel < 4 * 3; ++pixel )
    {
        const u8* rgba = pixels.data() + pixel * 4;
        CHECK( rgba[0] == 255 );
        CHECK( abs( rgba[1] - 128 ) <= 1 );
        CHECK( rgba[2] == 0 );
        CHECK( rgba[3] == 255 );
    }
}

TEST_CASE( "A window with nothing to draw into stops the GPU's start, and a start answers once" )
{
    const Scope<Platform> platform =
        Required( Platform::TryStart( PlatformOptions{ .mVideoDriver = "dummy", .mMouseAsTouch = false } ) );
    const Window window = Required( platform->TryOpenWindow() );

    const Scope<GpuStartup> startup = Gpu::Start( window );
    const auto finishedMaybe = startup->TryFinish();
    REQUIRE( finishedMaybe );
    CHECK( finishedMaybe->error() == "the window has nothing the GPU can draw into" );
    CHECK_THROWS_AS( startup->TryFinish(), logic_error );
}
#endif
