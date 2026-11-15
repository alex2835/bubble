#pragma once
#include "bubble/core/color.hpp"
#include "bubble/render/gpu/gpu_assets.hpp"
#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu_texture.hpp"
#include "bubble/render/render_scene.hpp"

namespace bubble
{
// Where a view is drawn: the window's surface for the frame, a texture of
// the editor's.
struct RenderTarget
{
    WGPUTextureView mView = nullptr;
    ivec2 mSize{ 0 };
    // The view's format: the last pass is made for it.
    WGPUTextureFormat mFormat = WGPUTextureFormat_Undefined;
};

struct RendererOptions
{
    // Behind everything, as picked.
    Color mBackground{ .mR = 0.45f, .mG = 0.6f, .mB = 0.8f, .mA = 1.0f };
    // What lights every surface from all around, as picked, times its
    // intensity: the sky, until there is one.
    Color mAmbient{ .mR = 0.6f, .mG = 0.7f, .mB = 0.9f, .mA = 1.0f };
    f32 mAmbientIntensity = 0.25f;
    u32 mShadowMapSize = 2048;
};

// Draws what a world gathered - its RenderScene - as one camera sees it,
// into a target: the shadowed directional light's shadow map, the lit
// surfaces in linear light (rgba16float, depth reversed), then tonemapping
// into the target. Models come as GpuAssets has them; one still loading is
// not drawn yet. One a process, after the Gpu, beside the registry.
class Renderer
{
public:
    // The Gpu and the registry outlive it.
    Renderer( Gpu& gpu, const AssetRegistry& assets, RendererOptions options = {} );
    ~Renderer();
    Renderer( const Renderer& ) = delete;
    Renderer& operator=( const Renderer& ) = delete;

    void Draw( const RenderScene& scene, const RenderCamera& camera, const RenderTarget& target );

    GpuAssets& Assets() { return mAssets; }

    // The shader's own layouts, kept in step with standard.wgsl.
    struct FrameUniforms;
    struct LightData;
    struct ObjectData;

private:
    struct DrawCall
    {
        const GpuModel& mModel;
        u32 mMesh = 0;
        u32 mObject = 0;
    };

    void Resize( ivec2 size );
    void Gather( const RenderScene& scene );
    // Grows a storage buffer to hold `bytes`, and says whether it did - its
    // bind group is made again then.
    bool Fit( GpuBuffer& buffer, u64 bytes, string_view label );
    void MakeBindGroups();
    WGPURenderPipeline TonemapFor( WGPUTextureFormat format );

    Gpu& mGpu;
    GpuAssets mAssets;
    RendererOptions mOptions;

    GpuShaderHandle mStandard;
    GpuShaderHandle mShadow;
    GpuShaderHandle mTonemap;
    GpuBindGroupLayoutHandle mFrameLayout;
    GpuBindGroupLayoutHandle mObjectsLayout;
    GpuBindGroupLayoutHandle mShadowFrameLayout;
    GpuBindGroupLayoutHandle mTonemapLayout;
    GpuRenderPipelineHandle mStandardPipeline;
    GpuRenderPipelineHandle mShadowPipeline;
    hmap<u32, GpuRenderPipelineHandle> mTonemapPipelines;

    GpuSamplerHandle mShadowSampler;
    GpuTexture mShadowMap;
    GpuBuffer mFrame;
    GpuBuffer mShadowFrame;
    GpuBuffer mLights;
    GpuBuffer mObjects;
    GpuBindGroupHandle mFrameGroup;
    GpuBindGroupHandle mShadowFrameGroup;
    GpuBindGroupHandle mObjectsGroup;

    // Sized to the target.
    GpuTexture mHdr;
    GpuTexture mDepth;
    GpuBindGroupHandle mTonemapGroup;

    // This frame's, kept for their room.
    vector<ObjectData> mObjectData;
    vector<LightData> mLightData;
    vector<DrawCall> mDraws;
    vec3 mBoundsMin{ 0.0f };
    vec3 mBoundsMax{ 0.0f };
};
}
