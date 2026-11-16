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
    // How far, in pixels, a level of detail may stray from the full mesh
    // on screen: the coarsest that keeps within it is drawn.
    f32 mLodPixels = 1.0f;
};

// What the last Draw drew: for tests, and the editor's overlay.
struct RenderStats
{
    // The items whose model has come.
    u32 mItems = 0;
    // Those in the camera's view.
    u32 mVisible = 0;
    // The camera's draw calls, the instances they drew, and their
    // triangles.
    u32 mDrawCalls = 0;
    u32 mInstances = 0;
    u64 mTriangles = 0;
    u32 mShadowDrawCalls = 0;
};

class GpuMaterials;
struct GpuMaterial;

// One place a world is seen from, drawn each frame: the game's window, the
// editor's viewport, a material's preview. What lasts from one of its
// frames to the next - targets of its size, the level each of its meshes
// was at, a pick on its way, what it drew - is its own, so views drawn in
// one frame, of one world or of several, never disturb each other. Made
// before the renderer draws it, gone before the Gpu.
class RenderView
{
public:
    RenderView();
    ~RenderView();
    RenderView( const RenderView& ) = delete;
    RenderView& operator=( const RenderView& ) = delete;

    // Asks which entity the next Draw of this view shows at `pixel` of its
    // target, from the top left: TryPicked gives it once it has come back
    // from the GPU - when the GPU has drawn that frame: a frame or so later
    // on a real one, more on a slow one. A new ask drops one still on its
    // way.
    void Pick( ivec2 pixel );
    // The entity asked for, once: Entity::Null where nothing was drawn;
    // nothing while it has not come back.
    opt<Entity> TryPicked();

    const RenderStats& Stats() const { return mStats; }

    // A pick on its way back from the GPU: shared with the map's callback.
    struct PickState;

private:
    friend class Renderer;

    // A mesh's level the frames before: a mesh grows coarser only once its
    // error is well within the limit, so one at the limit does not flicker.
    struct LodChoice
    {
        u32 mLod = 0;
        u64 mFrame = 0;
    };

    // Sized to the view's target.
    GpuTexture mHdr;
    GpuTexture mDepth;
    GpuBindGroupHandle mTonemapGroup;
    // Made at the first pick.
    GpuTexture mPickIds;
    opt<ivec2> mPickWanted;
    GpuBuffer mPickBuffer;
    Ref<PickState> mPicking;
    hmap<u64, LodChoice> mLods;
    u64 mFrames = 0;
    RenderStats mStats;
};

// Draws what a world gathered - its RenderScene - as one camera sees it,
// into a view's target: the shadowed directional light's shadow map, the
// opaque surfaces in linear light (rgba16float, depth reversed), the
// transparent ones over them from the farthest, the debug drawing's lines,
// then tonemapping into the target. Models come as GpuAssets has them - one
// still loading is not drawn yet - and each mesh with its material, which
// loads what it needs from the registry. What is out of view is not drawn,
// each mesh at the level of detail its size on screen needs, and meshes
// alike - model, mesh, level, material - in one call. One a process, after
// the Gpu, beside the registry; each Draw is submitted whole, so views
// follow one another on the GPU as they were drawn.
class Renderer
{
public:
    // The Gpu and the registry outlive it.
    Renderer( Gpu& gpu, AssetRegistry& assets, RendererOptions options = {} );
    ~Renderer();
    Renderer( const Renderer& ) = delete;
    Renderer& operator=( const Renderer& ) = delete;

    void Draw( RenderView& view, const RenderScene& scene, const RenderCamera& camera, const RenderTarget& target );

    GpuAssets& Assets() { return mAssets; }

    // The shaders' own layouts, kept in step with lit.wgsl.
    struct FrameUniforms;
    struct LightData;
    struct ObjectData;
    // An item whose model has come, as this frame takes it.
    struct Item;
    struct LineVertex;

private:
    struct DrawCall
    {
        const GpuModel& mModel;
        const GpuMaterial& mMaterial;
        u32 mMesh = 0;
        u32 mLod = 0;
        u32 mItem = 0;
        // From the camera to the item, squared: transparent ones are drawn
        // from the farthest.
        f32 mDistance = 0;
    };

    // Draws in a row that are one call: instances from `mFirstInstance` on.
    struct Batch
    {
        size_t mDraw = 0;
        u32 mFirstInstance = 0;
        u32 mInstances = 0;
    };

    void Resize( RenderView& view, ivec2 size );
    void Collect( const RenderScene& scene );
    u32 LodOf( RenderView& view, const Item& item, u32 mesh, vec3 eye, f32 pixelsAtOne );
    // The draws in order and as calls: the opaque grouped by pipeline,
    // material, model, mesh and level, one call for each run alike; the
    // transparent after, the farthest first, a call each. Instances go to
    // the object data in the order they are drawn.
    void Order( const vector<DrawCall>& draws, vector<u32>& order, vector<Batch>& batches, bool shadow );
    // Grows a storage buffer to hold `bytes`, and says whether it did - its
    // bind group is made again then.
    bool Fit( GpuBuffer& buffer, u64 bytes, string_view label, WGPUBufferUsage usage = WGPUBufferUsage_Storage );
    void EncodePick( RenderView& view, WGPUCommandEncoder encoder, ivec2 size );
    void MakeBindGroups();
    WGPURenderPipeline TonemapFor( WGPUTextureFormat format );

    Gpu& mGpu;
    GpuAssets mAssets;
    RendererOptions mOptions;

    GpuShaderHandle mShadow;
    GpuShaderHandle mTonemap;
    GpuShaderHandle mLines;
    GpuShaderHandle mPick;
    GpuBindGroupLayoutHandle mFrameLayout;
    GpuBindGroupLayoutHandle mObjectsLayout;
    GpuBindGroupLayoutHandle mShadowFrameLayout;
    GpuBindGroupLayoutHandle mTonemapLayout;
    GpuRenderPipelineHandle mShadowPipeline;
    GpuRenderPipelineHandle mLinePipeline;
    // One side, two sides.
    array<GpuRenderPipelineHandle, 2> mPickPipelines;
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

    GpuBuffer mLineVertices;

    // After the layouts it builds its pipelines with.
    Scope<GpuMaterials> mMaterials;

    // This frame's, kept for their room.
    vector<Item> mItems;
    vector<ObjectData> mObjectData;
    vector<LightData> mLightData;
    vector<DrawCall> mDraws;
    vector<DrawCall> mShadowDraws;
    vector<u32> mOrder;
    vector<u32> mShadowOrder;
    vector<Batch> mBatches;
    vector<Batch> mShadowBatches;
    vec3 mBoundsMin{ 0.0f };
    vec3 mBoundsMax{ 0.0f };
    vector<LineVertex> mLineData;
};
}
