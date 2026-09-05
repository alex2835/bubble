#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/pointer.hpp"
#include "engine/renderer/webgpu.hpp"

struct GLFWwindow;

namespace bubble
{
class StandardLayouts;
class Texture2D;

// Owns the WebGPU objects that live for the whole program: instance, adapter,
// device, queue, and the window surface.
//
// Member order matters - destruction runs in reverse, so the surface is released
// before the instance that created it, and the frame's texture before both.
class GpuContext
{
public:
    explicit GpuContext( GLFWwindow* window );
    ~GpuContext();

    GpuContext( const GpuContext& ) = delete;
    GpuContext& operator=( const GpuContext& ) = delete;

    wgpu::Device Device() const { return *mDevice; }
    wgpu::Queue Queue() const { return *mQueue; }
    wgpu::Surface Surface() const { return *mSurface; }

    // Format of the window's backbuffer. Any pipeline drawing into it - which
    // in practice means only ImGui - must declare this as its color target.
    wgpu::TextureFormat SurfaceFormat() const { return mSurfaceFormat; }

    // (Re)configure the surface, on resize. A zero width or height means the
    // window is minimized: the surface is left unconfigured until it comes back,
    // since configuring a zero-sized surface is a validation error.
    void ConfigureSurface( uvec2 size );

    // Fifo is vsync; Immediate is not. Reconfigures the surface. A mode the
    // surface does not support is ignored with a warning rather than failing -
    // Immediate in particular is unavailable on some platforms.
    void SetPresentMode( wgpu::PresentMode mode );

    // The view to render this frame's UI into. Empty when there is nothing to
    // draw to (minimized, or the surface went stale and was reconfigured) - the
    // caller must check and skip the frame.
    wgpu::raii::TextureView AcquireSurfaceView();

    void Present();

    // wgpu-native runs device callbacks (uncaptured errors, buffer maps) only
    // when polled. Call once per frame or errors surface late, or never.
    void PollDevice();

    // The bind group and pipeline layouts every pipeline shares. Built on first
    // use rather than in the constructor, because creating them needs the
    // device to already be reachable through Gpu().
    StandardLayouts& Layouts();

    // 1x1 opaque white, bound wherever a material has no map of that kind. A
    // bind group has to fill every entry its layout declares, and this is much
    // cheaper than a pipeline variant per combination of present maps.
    Texture2D& WhiteTexture();

private:
    void LogAdapterInfo() const;

    wgpu::raii::Instance mInstance;
    wgpu::raii::Surface  mSurface;
    wgpu::raii::Adapter  mAdapter;
    wgpu::raii::Device   mDevice;
    wgpu::raii::Queue    mQueue;

    wgpu::TextureFormat mSurfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::PresentMode mPresentMode = wgpu::PresentMode::Fifo;
    uvec2 mSize = uvec2( 0u );
    bool mConfigured = false;

    // The frame's surface texture, held from acquisition until Present.
    // Releasing it earlier destroys it out from under the view referenced by
    // commands that have not been submitted yet. Declared last so it goes first.
    wgpu::raii::Texture mCurrentTexture;

    Scope<StandardLayouts> mLayouts;
    Scope<Texture2D> mWhiteTexture;
};


// The process-wide context.
//
// OpenGL had exactly one current context and every renderer class here assumed
// it. WebGPU makes the device explicit, but threading it through the constructor
// of every Texture2D, VertexBuffer, Shader and Framebuffer - and through the
// loaders and the editor that build them - would be a very large diff to encode
// something that is still, in this engine, a single global. So the shape is
// kept: Window creates the context before anything else and destroys it after,
// and resources reach it through here.
//
// Valid only between Window's constructor and destructor. BubbleEditor declares
// mWindow ahead of mEngine and mProject, so every GPU resource is already gone
// by the time the context is torn down.
GpuContext& Gpu();

// Called by Window. Not for anyone else.
void SetGpuContext( GpuContext* context );

}
