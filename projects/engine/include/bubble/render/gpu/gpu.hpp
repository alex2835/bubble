#pragma once
#include "bubble/core/color.hpp"
#include "bubble/platform/window.hpp"
#include "bubble/render/gpu/gpu_handle.hpp"
#include "bubble/types/containers.hpp"
#include "bubble/types/function.hpp"
#include "bubble/types/opt_ref.hpp"
#include "bubble/types/pointer.hpp"

namespace bubble
{
struct GpuOptions
{
    // The system's software adapter - WARP on Windows, lavapipe on Linux -
    // for tests on a machine with no GPU.
    bool mFallbackAdapter = false;
};

// A window's swap chain: the texture each frame draws into and shows. The
// Gpu that made it owns it.
class GpuSurface
{
public:
    ~GpuSurface();
    GpuSurface( GpuSurface&& ) noexcept = default;
    GpuSurface& operator=( GpuSurface&& ) noexcept = default;

    // What the surface's textures are; their views - what a pass draws
    // into - are its sRGB twin when there is one.
    WGPUTextureFormat Format() const { return mFormat; }
    // What TryAcquire's views are: the sRGB twin of Format.
    WGPUTextureFormat ViewFormat() const;
    ivec2 Size() const { return mSize; }
    // Configured for the window's pixels when they changed. 0 wide or high -
    // minimized - leaves it unconfigured until they come back.
    void Resize( ivec2 pixels );
    // What this frame draws into, or nothing - minimized, or out of date and
    // configured again - and the frame draws nothing.
    opt<GpuTextureViewHandle> TryAcquire();
    // Shows what the frame drew; after TryAcquire gave a view.
    void Present();

private:
    friend class GpuStartup;
    GpuSurface( GpuSurfaceHandle surface, WGPUDevice device, WGPUTextureFormat format );
    void Configure();

    GpuSurfaceHandle mSurface;
    // The Gpu's, which outlives its surface.
    WGPUDevice mDevice = nullptr;
    WGPUTextureFormat mFormat = WGPUTextureFormat_Undefined;
    ivec2 mSize{ 0 };
    bool mConfigured = false;
    // The frame's texture, from TryAcquire to Present: released before, it
    // would go from under commands not yet submitted.
    GpuTextureHandle mCurrent;
};

class Gpu;
class GpuTexture;

// What the GPU said of the calls made inside a Gpu::Check: nothing wrong, or
// the first validation error - a WGSL mistake with its line, a pipeline that
// does not fit its shader. It answers at a Poll: the next one on the
// desktop, a later frame's in a browser. Copies share the answer.
class GpuCheck
{
public:
    // The answer once it came, and from then on.
    opt<expected<void, string>> TryResult() const;

    struct State;

private:
    friend class Gpu;
    explicit GpuCheck( Ref<State> state ) : mState( std::move( state ) ) {}

    Ref<State> mState;
};

// A Gpu on its way: the system is asked for an adapter and a device, and
// answers at once on the desktop, at a later frame in a browser.
class GpuStartup
{
public:
    ~GpuStartup();
    GpuStartup( const GpuStartup& ) = delete;
    GpuStartup& operator=( const GpuStartup& ) = delete;

    // The Gpu once the system has answered, an error if it said no, nothing
    // while it has not: called each frame until it gives one or the other.
    // After that it is spent, and a call is a logic_error.
    opt<expected<Scope<Gpu>, string>> TryFinish();

    struct State;

private:
    friend class Gpu;
    explicit GpuStartup( Ref<State> state ) : mState( std::move( state ) ) {}

    Ref<State> mState;
};

// WebGPU's device and its queue, and the window's surface if it was started
// with one: one a process, made by the app after the window, gone before
// it. WebGPU's own types stop at bubble/render/gpu.
class Gpu
{
public:
    // Asks for a GPU that can draw into `window`, or into textures alone.
    static Scope<GpuStartup> Start( OptRef<const Window> window, GpuOptions options = {} );
#ifndef __EMSCRIPTEN__
    // The same, waited for: tests and tools. Not in a browser, which answers
    // only between frames.
    static expected<Scope<Gpu>, string> TryStartNow( OptRef<const Window> window, GpuOptions options = {} );
#endif
    ~Gpu();
    Gpu( const Gpu& ) = delete;
    Gpu& operator=( const Gpu& ) = delete;

    WGPUInstance Instance() const { return mInstance.Get(); }
    WGPUDevice Device() const { return mDevice.Get(); }
    WGPUQueue Queue() const { return mQueue.Get(); }
    // "NVIDIA GeForce RTX 4070, Vulkan, discrete GPU".
    const string& Describe() const { return mDescription; }
    OptRef<GpuSurface> TrySurface();

    // Runs `make` with the GPU's validation errors kept for the check
    // rather than logged.
    GpuCheck Check( const function<void()>& make );
#ifndef __EMSCRIPTEN__
    // The same, waited for: tests and tools.
    expected<void, string> TryCheckNow( const function<void()>& make );
#endif

    // Finishes the encoder's commands and hands them to the queue.
    void Submit( const GpuCommandEncoderHandle& encoder );
    // Fills `target` with one color, as picked: in sRGB, which an sRGB target
    // stores as it is.
    void Clear( WGPUTextureView target, Color color );
    // Hands over what the device has finished - errors, buffers read back -
    // to their callbacks. Once a frame, or they come late, or never.
    void Poll();
#ifndef __EMSCRIPTEN__
    // One level of an RGBA8 texture made with CopySrc: its pixels, row by
    // row from the top, once the GPU has drawn them - screenshots and the
    // tests against a reference picture. Another format is a logic_error.
    expected<vector<u8>, string> TryReadRgba8( const GpuTexture& texture, u32 mip = 0 );
#endif

private:
    friend class GpuStartup;
    Gpu() = default;

    GpuInstanceHandle mInstance;
    GpuAdapterHandle mAdapter;
    GpuDeviceHandle mDevice;
    GpuQueueHandle mQueue;
    string mDescription;
    // Last, so it goes first: before the device and the instance it is of.
    opt<GpuSurface> mSurface;
};
}
