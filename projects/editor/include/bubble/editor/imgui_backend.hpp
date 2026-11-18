#pragma once
#include "bubble/input/input_event.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu_texture.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include <imgui.h>

// Dear ImGui's backends, our own: the platform's InputEvents in, bubble's GPU
// out. ImGui sees neither SDL nor WebGPU, and the editor runs wherever the
// engine does.
namespace bubble
{
// An ImGui context, current while it lives: the editor's, a test's. Its
// layout is kept in `layoutFile` - none for a test.
class ImGuiContextScope
{
public:
    explicit ImGuiContextScope( const OsPath& layoutFile = {} );
    ~ImGuiContextScope();
    ImGuiContextScope( const ImGuiContextScope& ) = delete;
    ImGuiContextScope& operator=( const ImGuiContextScope& ) = delete;

private:
    ImGuiContext* mContext = nullptr;
    // ImGui keeps the pointer, not the text.
    string mLayoutFile;
};

// ImGui's platform side: what the player did, as the platform polled it;
// the window's size, scale and time a frame; the pointer's shape, text input
// and the clipboard back through the platform and the window. Made after
// the context, gone before it; the platform and the window outlive it.
class ImGuiInput
{
public:
    ImGuiInput( Platform& platform, const Window& window );
    ~ImGuiInput();
    ImGuiInput( const ImGuiInput& ) = delete;
    ImGuiInput& operator=( const ImGuiInput& ) = delete;

    // An event for ImGui, as it came.
    void Handle( const InputEvent& event );
    // Before ImGui::NewFrame: the window as it is now, `elapsed` seconds
    // after the frame before.
    void NewFrame( f64 elapsed );
    // After the frame's widgets: the pointer and the keyboard as ImGui wants
    // them - a text field takes text.
    void EndFrame();

private:
    void Modifier( ImGuiKey modifier, Key left, Key right );

    Platform& mPlatform;
    const Window& mWindow;
    // ImGui keeps the pointer it was given last.
    string mClipboard;
    array<bool, cKeySlots> mKeys{};
};

// ImGui's renderer on bubble's GPU: the textures its draw data asks for -
// the font atlas, which grows as text needs glyphs - made, updated and let
// go, and its lists drawn over a target, which is cleared first. Targets
// are sRGB, as the window's views are: ImGui's colours are taken as sRGB and
// blended in linear light. A texture of ours in a panel - the viewport's
// picture - goes into ImGui::Image as TextureId( view ). Made after the
// context and the Gpu, gone before them.
class ImGuiRenderer
{
public:
    explicit ImGuiRenderer( Gpu& gpu );
    ~ImGuiRenderer();
    ImGuiRenderer( const ImGuiRenderer& ) = delete;
    ImGuiRenderer& operator=( const ImGuiRenderer& ) = delete;

    void Draw( ImDrawData& data, const RenderTarget& target, Color clear );

    static ImTextureID TextureId( WGPUTextureView view );

private:
    // One of ImGui's own textures, by the id it was given.
    struct Managed
    {
        GpuTexture mTexture;
        GpuBindGroupHandle mGroup;
    };

    void Update( ImTextureData& texture );
    // Made for a target's format as it first comes: the window's, a
    // screenshot's.
    WGPURenderPipeline PipelineFor( WGPUTextureFormat format );
    WGPUBindGroup GroupFor( ImTextureID id );
    void Fit( GpuBuffer& buffer, u64 size, WGPUBufferUsage usage, string_view label );

    Gpu& mGpu;
    GpuShaderHandle mShader;
    GpuBindGroupLayoutHandle mFrameLayout;
    GpuBindGroupLayoutHandle mImageLayout;
    hmap<u32, GpuRenderPipelineHandle> mPipelines;
    GpuSamplerHandle mSampler;
    GpuBuffer mUniforms;
    GpuBindGroupHandle mFrameGroup;
    GpuBuffer mVertices;
    GpuBuffer mIndices;
    vector<ImDrawVert> mVertexData;
    vector<ImDrawIdx> mIndexData;
    hmap<ImTextureID, Managed> mManaged;
    // Textures of ours, by view: made again each frame, as a view may go -
    // a viewport resized - and another come where it was.
    hmap<ImTextureID, GpuBindGroupHandle> mFrameGroups;
};
}
