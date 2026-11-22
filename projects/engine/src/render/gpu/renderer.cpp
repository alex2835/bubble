#include "bubble/render/gpu/renderer.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_binding.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "gpu_materials.hpp"
#include <glm/ext/matrix_clip_space.hpp>

namespace bubble
{
struct Renderer::FrameUniforms
{
    mat4 mViewProjection{ 1.0f };
    mat4 mShadowViewProjection{ 1.0f };
    vec3 mCameraPosition{ 0.0f };
    u32 mLightCount = 0;
    vec3 mAmbient{ 0.0f };
    u32 mShadowLight = 0;
    f32 mShadowNormalOffset = 0;
    f32 mTime = 0;
    array<f32, 2> mPadding{};
};
static_assert( sizeof( Renderer::FrameUniforms ) == 176 );
static_assert( offsetof( Renderer::FrameUniforms, mCameraPosition ) == 128 );
static_assert( offsetof( Renderer::FrameUniforms, mAmbient ) == 144 );
static_assert( offsetof( Renderer::FrameUniforms, mShadowNormalOffset ) == 160 );
static_assert( offsetof( Renderer::FrameUniforms, mTime ) == 164 );

struct Renderer::LightData
{
    vec3 mPosition{ 0.0f };
    f32 mRange = 0;
    vec3 mDirection{ 0.0f };
    f32 mInnerCos = -1;
    vec3 mRadiance{ 0.0f };
    f32 mOuterCos = -1;
    u32 mDirectional = 0;
    array<u32, 3> mPadding{};
};
static_assert( sizeof( Renderer::LightData ) == 64 );
static_assert( offsetof( Renderer::LightData, mRadiance ) == 32 );

struct Renderer::ObjectData
{
    mat4 mModel{ 1.0f };
    mat4 mNormal{ 1.0f };
    vec4 mTint{ 1.0f };
    u32 mEntity = 0;
    // Where a skinned mesh's joint matrices start in the frame's skins.
    u32 mSkin = 0;
    array<u32, 2> mPadding{};
};
static_assert( sizeof( Renderer::ObjectData ) == 160 );
static_assert( offsetof( Renderer::ObjectData, mEntity ) == 144 );
static_assert( offsetof( Renderer::ObjectData, mSkin ) == 148 );

struct Renderer::LineVertex
{
    vec3 mPosition{ 0.0f };
    vec4 mColor{ 1.0f };
};
static_assert( sizeof( Renderer::LineVertex ) == 28 );

struct Renderer::UiVertex
{
    vec2 mPosition{ 0.0f };
    vec2 mUv{ 0.0f };
    vec4 mColor{ 1.0f };
    u32 mCanvas = 0;
    u32 mKind = 0;
    f32 mRange = 0;
    f32 mPadding = 0;
};
static_assert( sizeof( Renderer::UiVertex ) == 48 );

struct RenderView::PickState
{
    bool mDone = false;
    bool mMapped = false;
};

struct Renderer::Item
{
    const RenderItem& mSource;
    const GpuModel& mModel;
    ObjectData mObject;
    // Its box in the world, and the sphere around that box.
    vec3 mMin{ 0.0f };
    vec3 mMax{ 0.0f };
    vec3 mCentre{ 0.0f };
    f32 mRadius = 0;
    // How much its world matrix grows its model, at most.
    f32 mScale = 1;
};

namespace
{
// No light has it: no shadow this frame.
constexpr u32 cNoShadow = ~0u;
constexpr WGPUTextureFormat cPickFormat = WGPUTextureFormat_R32Uint;
// A copy's rows are this far apart at least: room for the one texel read.
constexpr u64 cPickBytes = 256;

void OnPickMapped( WGPUMapAsyncStatus status, WGPUStringView, void* held, void* )
{
    const Scope<Ref<RenderView::PickState>> state( static_cast<Ref<RenderView::PickState>*>( held ) );
    ( *state )->mDone = true;
    ( *state )->mMapped = status == WGPUMapAsyncStatus_Success;
}

// The view and projection a camera sees through, depth reversed: the
// near plane at 1, the far one at 0 - glm's own projection with the two
// swapped.
mat4 ViewProjection( const RenderCamera& camera, ivec2 size )
{
    const f32 aspect = static_cast<f32>( size.x ) / static_cast<f32>( size.y );
    return glm::perspectiveRH_ZO( camera.mFov, aspect, camera.mFar, camera.mNear ) * glm::inverse( camera.mWorld );
}

// A directional light's view of everything there is to draw, in a box
// around it, depth reversed as the camera's is.
mat4 ShadowViewProjection( vec3 direction, vec3 boundsMin, vec3 boundsMax, f32& texelSize, u32 mapSize )
{
    const vec3 centre = ( boundsMin + boundsMax ) * 0.5f;
    const f32 radius = max( glm::length( boundsMax - boundsMin ) * 0.5f, 0.01f );
    // Looking straight up or down, Y is no up.
    const vec3 up = abs( direction.y ) > 0.99f ? vec3( 0.0f, 0.0f, 1.0f ) : vec3( 0.0f, 1.0f, 0.0f );
    const mat4 view = glm::lookAtRH( centre - direction * radius * 2.0f, centre, up );
    texelSize = radius * 2.0f / static_cast<f32>( mapSize );
    return glm::orthoRH_ZO( -radius, radius, -radius, radius, radius * 4.0f, 0.0f ) * view;
}

// The planes a view-projection's clip space is bounded by, each facing in:
// x and y from -w to w, depth from 0 to w.
struct Frustum
{
    array<vec4, 6> mPlanes;
};

Frustum FrustumOf( const mat4& viewProjection )
{
    const auto row = [&]( i32 at ) {
        return vec4( viewProjection[0][at], viewProjection[1][at], viewProjection[2][at], viewProjection[3][at] );
    };
    return Frustum{ .mPlanes = { row( 3 ) + row( 0 ), row( 3 ) - row( 0 ), row( 3 ) + row( 1 ), row( 3 ) - row( 1 ),
                                 row( 2 ), row( 3 ) - row( 2 ) } };
}

// Whether a box in the world may be seen: not wholly behind any plane.
bool Overlaps( const Frustum& frustum, vec3 min, vec3 max )
{
    for ( const vec4& plane : frustum.mPlanes )
    {
        // The box's corner farthest along the plane's normal.
        const vec3 corner( plane.x > 0 ? max.x : min.x, plane.y > 0 ? max.y : min.y, plane.z > 0 ? max.z : min.z );
        if ( glm::dot( vec3( plane ), corner ) + plane.w < 0.0f )
            return false;
    }
    return true;
}

// How a UI vertex lies: its position, uv, color, canvas, kind and range.
GpuVertexBufferLayout UiVertexLayout()
{
    const auto attribute = []( WGPUVertexFormat format, u64 offset, u32 location ) {
        WGPUVertexAttribute vertexAttribute = WGPU_VERTEX_ATTRIBUTE_INIT;
        vertexAttribute.format = format;
        vertexAttribute.offset = offset;
        vertexAttribute.shaderLocation = location;
        return vertexAttribute;
    };
    return GpuVertexBufferLayout{ .mStride = 48,
                                  .mAttributes =
                                      vector<WGPUVertexAttribute>{ attribute( WGPUVertexFormat_Float32x2, 0, 0 ),
                                                                   attribute( WGPUVertexFormat_Float32x2, 8, 1 ),
                                                                   attribute( WGPUVertexFormat_Float32x4, 16, 2 ),
                                                                   attribute( WGPUVertexFormat_Uint32, 32, 3 ),
                                                                   attribute( WGPUVertexFormat_Uint32, 36, 4 ),
                                                                   attribute( WGPUVertexFormat_Float32, 40, 5 ) } };
}

// The vertex buffers a model's mesh is drawn from: its vertices, and its
// skin beside them when it is skinned.
vector<GpuVertexBufferLayout> VertexLayouts( bool skinned )
{
    if ( skinned )
        return vector<GpuVertexBufferLayout>{ ModelVertexLayout(), ModelSkinLayout() };
    return vector<GpuVertexBufferLayout>{ ModelVertexLayout() };
}

// Where an item's model may reach in the world: its box's corners, moved.
void Grow( vec3& boundsMin, vec3& boundsMax, const mat4& world, vec3 min, vec3 max )
{
    for ( u32 corner = 0; corner < 8; ++corner )
    {
        const vec3 local( corner & 1 ? max.x : min.x, corner & 2 ? max.y : min.y, corner & 4 ? max.z : min.z );
        const vec3 point = vec3( world * vec4( local, 1.0f ) );
        boundsMin = glm::min( boundsMin, point );
        boundsMax = glm::max( boundsMax, point );
    }
}
}

Renderer::Renderer( Gpu& gpu, AssetRegistry& assets, RendererOptions options )
    : mGpu( gpu ),
      mAssets( gpu, assets ),
      mOptions( options )
{
    mShadow = CreateShader( gpu, EngineShader( "shadow.wgsl" ), "shadow.wgsl" );
    mTonemap = CreateShader( gpu, EngineShader( "tonemap.wgsl" ), "tonemap.wgsl" );
    mLines = CreateShader( gpu, EngineShader( "debug_lines.wgsl" ), "debug_lines.wgsl" );
    mPick = CreateShader( gpu, EngineShader( "pick.wgsl" ), "pick.wgsl" );
    mUi = CreateShader( gpu, EngineShader( "ui.wgsl" ), "ui.wgsl" );

    const WGPUShaderStage both = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    mFrameLayout = CreateLayout( gpu, "frame",
                                 vector<WGPUBindGroupLayoutEntry>{
                                     BufferEntry( 0, both, WGPUBufferBindingType_Uniform ),
                                     BufferEntry( 1, WGPUShaderStage_Fragment, WGPUBufferBindingType_ReadOnlyStorage ),
                                     TextureEntry( 2, WGPUTextureSampleType_Depth ),
                                     SamplerEntry( 3, WGPUSamplerBindingType_Comparison ),
                                 } );
    // The vertex stage reads an object's matrices and its skin's, the
    // fragment its tint.
    mObjectsLayout =
        CreateLayout( gpu, "objects",
                      vector<WGPUBindGroupLayoutEntry>{
                          BufferEntry( 0, both, WGPUBufferBindingType_ReadOnlyStorage ),
                          BufferEntry( 1, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage ) } );
    mShadowFrameLayout = CreateLayout(
        gpu, "shadow frame",
        vector<WGPUBindGroupLayoutEntry>{ BufferEntry( 0, WGPUShaderStage_Vertex, WGPUBufferBindingType_Uniform ) } );
    mTonemapLayout = CreateLayout( gpu, "tonemap",
                                   vector<WGPUBindGroupLayoutEntry>{ TextureEntry( 0, WGPUTextureSampleType_Float ) } );

    // Depth is reversed: pushing a caster away from the light is a bias
    // down.
    for ( const bool skinned : { false, true } )
        mShadowPipelines[skinned ? 1 : 0] = CreateRenderPipeline(
            gpu, GpuPipelineOptions{ .mLabel = "shadow",
                                     .mShader = mShadow.Get(),
                                     .mVertexEntry = skinned ? "vs_skinned" : "vs_main",
                                     .mFragmentEntry = "",
                                     .mBindGroupLayouts = { mShadowFrameLayout.Get(), mObjectsLayout.Get() },
                                     .mVertexBuffers = VertexLayouts( skinned ),
                                     .mColorFormat = WGPUTextureFormat_Undefined,
                                     .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat,
                                                                .mCompare = WGPUCompareFunction_Greater,
                                                                .mWrite = true,
                                                                .mBias = -2,
                                                                .mBiasSlopeScale = -2.0f },
                                     .mTopology = WGPUPrimitiveTopology_TriangleList,
                                     // Both sides cast: a two-sided leaf as much
                                     // as a closed box, whose far side the
                                     // near one hides from the light anyway.
                                     .mCullMode = WGPUCullMode_None,
                                     .mBlend = false } );

    const auto lineAttribute = []( WGPUVertexFormat format, u64 offset, u32 location ) {
        WGPUVertexAttribute attribute = WGPU_VERTEX_ATTRIBUTE_INIT;
        attribute.format = format;
        attribute.offset = offset;
        attribute.shaderLocation = location;
        return attribute;
    };
    mLinePipeline = CreateRenderPipeline(
        gpu,
        GpuPipelineOptions{
            .mLabel = "debug lines",
            .mShader = mLines.Get(),
            .mVertexEntry = "vs_main",
            .mFragmentEntry = "fs_main",
            .mBindGroupLayouts = { mFrameLayout.Get() },
            .mVertexBuffers = { GpuVertexBufferLayout{
                .mStride = sizeof( LineVertex ),
                .mAttributes = { lineAttribute( WGPUVertexFormat_Float32x3, offsetof( LineVertex, mPosition ), 0 ),
                                 lineAttribute( WGPUVertexFormat_Float32x4, offsetof( LineVertex, mColor ), 1 ) } } },
            .mColorFormat = cHdrFormat,
            .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat,
                                       .mCompare = WGPUCompareFunction_Greater,
                                       .mWrite = false,
                                       .mBias = 0,
                                       .mBiasSlopeScale = 0 },
            .mTopology = WGPUPrimitiveTopology_LineList,
            .mCullMode = WGPUCullMode_None,
            .mBlend = true } );
    for ( const bool skinned : { false, true } )
        for ( const bool doubleSided : { false, true } )
            mPickPipelines[( skinned ? 2 : 0 ) + ( doubleSided ? 1 : 0 )] = CreateRenderPipeline(
                gpu, GpuPipelineOptions{ .mLabel = "pick",
                                         .mShader = mPick.Get(),
                                         .mVertexEntry = skinned ? "vs_skinned" : "vs_main",
                                         .mFragmentEntry = "fs_main",
                                         .mBindGroupLayouts = { mFrameLayout.Get(), mObjectsLayout.Get() },
                                         .mVertexBuffers = VertexLayouts( skinned ),
                                         .mColorFormat = cPickFormat,
                                         .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat },
                                         .mTopology = WGPUPrimitiveTopology_TriangleList,
                                         .mCullMode = doubleSided ? WGPUCullMode_None : WGPUCullMode_Back,
                                         .mBlend = false } );

    // The UI: its canvases' matrices, then each batch's texture.
    mUiCanvasLayout = CreateLayout( gpu, "ui canvases",
                                    vector<WGPUBindGroupLayoutEntry>{ BufferEntry(
                                        0, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage ) } );
    mUiImageLayout =
        CreateLayout( gpu, "ui image",
                      vector<WGPUBindGroupLayoutEntry>{ TextureEntry( 0, WGPUTextureSampleType_Float ),
                                                        SamplerEntry( 1, WGPUSamplerBindingType_Filtering ) } );
    // In the world among the surfaces: behind what is nearer, hiding nothing.
    mUiWorldPipeline = CreateRenderPipeline(
        gpu, GpuPipelineOptions{ .mLabel = "ui in the world",
                                 .mShader = mUi.Get(),
                                 .mVertexEntry = "vs_main",
                                 .mFragmentEntry = "fs_main",
                                 .mBindGroupLayouts = { mUiCanvasLayout.Get(), mUiImageLayout.Get() },
                                 .mVertexBuffers = { UiVertexLayout() },
                                 .mColorFormat = cHdrFormat,
                                 .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat,
                                                            .mCompare = WGPUCompareFunction_Greater,
                                                            .mWrite = false,
                                                            .mBias = 0,
                                                            .mBiasSlopeScale = 0 },
                                 .mTopology = WGPUPrimitiveTopology_TriangleList,
                                 .mCullMode = WGPUCullMode_None,
                                 .mBlend = true } );
    WGPUSamplerDescriptor uiSampler = WGPU_SAMPLER_DESCRIPTOR_INIT;
    uiSampler.label = ToGpuString( "ui" );
    uiSampler.magFilter = WGPUFilterMode_Linear;
    uiSampler.minFilter = WGPUFilterMode_Linear;
    uiSampler.mipmapFilter = WGPUMipmapFilterMode_Linear;
    mUiSampler = GpuSamplerHandle( wgpuDeviceCreateSampler( gpu.Device(), &uiSampler ) );
    mUiWhite = GpuTexture( gpu, GpuTextureOptions{ .mSize = ivec2( 1 ), .mMips = 1, .mLabel = "ui white" } );
    const array<u8, 4> white{ 255, 255, 255, 255 };
    mUiWhite.Write( gpu, 0, AsBytes( white ) );
    Fit( mUiCanvases, sizeof( mat4 ), "ui canvases" );
    Fit( mUiVertices, sizeof( UiVertex ), "ui vertices", WGPUBufferUsage_Vertex );

    WGPUSamplerDescriptor sampler = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler.label = ToGpuString( "shadow" );
    sampler.magFilter = WGPUFilterMode_Linear;
    sampler.minFilter = WGPUFilterMode_Linear;
    sampler.compare = WGPUCompareFunction_GreaterEqual;
    mShadowSampler = GpuSamplerHandle( wgpuDeviceCreateSampler( gpu.Device(), &sampler ) );
    mShadowMap = GpuTexture(
        gpu, GpuTextureOptions{ .mSize = ivec2( static_cast<i32>( mOptions.mShadowMapSize ) ),
                                .mMips = 1,
                                .mFormat = cDepthFormat,
                                .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
                                .mLabel = "shadow map" } );
    mFrame = GpuBuffer( gpu, WGPUBufferUsage_Uniform, sizeof( FrameUniforms ), "frame" );
    mShadowFrame = GpuBuffer( gpu, WGPUBufferUsage_Uniform, sizeof( mat4 ), "shadow frame" );
    // Room for one of each: a storage binding is never empty.
    Fit( mLights, sizeof( LightData ), "lights" );
    Fit( mObjects, sizeof( ObjectData ), "objects" );
    Fit( mSkins, sizeof( mat4 ), "skins" );
    MakeBindGroups();
    mMaterials = CreateScope<GpuMaterials>( gpu, assets, mAssets, mFrameLayout.Get(), mObjectsLayout.Get() );
}

Renderer::~Renderer() = default;

bool Renderer::Fit( GpuBuffer& buffer, u64 bytes, string_view label, WGPUBufferUsage usage )
{
    if ( buffer and buffer.Size() >= bytes )
        return false;
    // Twice as much as asked: a scene that grows a little does not make it
    // again every frame.
    buffer = GpuBuffer( mGpu, usage, max( bytes * 2, buffer.Size() * 2 ), label );
    return true;
}

RenderView::RenderView() = default;
RenderView::~RenderView() = default;

void RenderView::Pick( ivec2 pixel )
{
    mPickWanted = pixel;
}

opt<Entity> RenderView::TryPicked()
{
    if ( not mPicking or not mPicking->mDone )
        return nullopt;
    Entity picked;
    if ( mPicking->mMapped )
    {
        array<byte, 4> texel{};
        const auto* mapped = static_cast<const byte*>(
            wgpuBufferGetConstMappedRange( mPickBuffer.Get(), 0, static_cast<size_t>( cPickBytes ) ) );
        if ( mapped != nullptr )
            copy_n( mapped, texel.size(), texel.data() );
        wgpuBufferUnmap( mPickBuffer.Get() );
        picked = Entity( bit_cast<u32>( texel ) );
    }
    mPicking.reset();
    return picked;
}

void Renderer::EncodePick( RenderView& view, WGPUCommandEncoder encoder, ivec2 size )
{
    const ivec2 pixel = glm::clamp( *view.mPickWanted, ivec2( 0 ), size - 1 );
    view.mPickWanted.reset();
    if ( not view.mPickIds or view.mPickIds.Size() != size )
        view.mPickIds =
            GpuTexture( mGpu, GpuTextureOptions{ .mSize = size,
                                                 .mMips = 1,
                                                 .mFormat = cPickFormat,
                                                 .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                                 .mLabel = "pick" } );
    {
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = view.mPickIds.View();
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        // Entity::Null where nothing is drawn.
        color.clearValue = WGPUColor{ static_cast<f64>( ~0u ), 0.0, 0.0, 0.0 };
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = view.mDepth.View();
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Discard;
        depth.depthClearValue = 0.0f;
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "pick" );
        descriptor.colorAttachmentCount = 1;
        descriptor.colorAttachments = &color;
        descriptor.depthStencilAttachment = &depth;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder, &descriptor ) );
        // The one pixel asked for is all that is shaded.
        wgpuRenderPassEncoderSetScissorRect( pass.Get(), static_cast<u32>( pixel.x ), static_cast<u32>( pixel.y ), 1,
                                             1 );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        OptRef<const GpuModel> boundModel;
        for ( const Batch& batch : mBatches )
        {
            const DrawCall& draw = mDraws[mOrder[batch.mDraw]];
            wgpuRenderPassEncoderSetPipeline(
                pass.Get(), mPickPipelines[( draw.mSkinned ? 2 : 0 ) + ( draw.mMaterial.mDoubleSided ? 1 : 0 )].Get() );
            if ( boundModel != OptRef<const GpuModel>( draw.mModel ) )
            {
                draw.mModel.Bind( pass.Get() );
                boundModel = draw.mModel;
            }
            draw.mModel.Draw( pass.Get(), draw.mMesh, draw.mLod, batch.mFirstInstance, batch.mInstances );
        }
        wgpuRenderPassEncoderEnd( pass.Get() );
    }
    // A new buffer for each pick: one still mapping for the last one goes
    // its own way.
    view.mPickBuffer = GpuBuffer( mGpu, WGPUBufferUsage_MapRead, cPickBytes, "pick" );
    WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    source.texture = view.mPickIds.Get();
    source.origin = WGPUOrigin3D{ static_cast<u32>( pixel.x ), static_cast<u32>( pixel.y ), 0 };
    WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    destination.buffer = view.mPickBuffer.Get();
    destination.layout.bytesPerRow = static_cast<u32>( cPickBytes );
    destination.layout.rowsPerImage = 1;
    const WGPUExtent3D extent{ 1, 1, 1 };
    wgpuCommandEncoderCopyTextureToBuffer( encoder, &source, &destination, &extent );
}

void Renderer::MakeBindGroups()
{
    mFrameGroup = CreateGroup( mGpu, "frame", mFrameLayout.Get(),
                               vector<WGPUBindGroupEntry>{ BufferBinding( 0, mFrame ), BufferBinding( 1, mLights ),
                                                           TextureBinding( 2, mShadowMap.View() ),
                                                           SamplerBinding( 3, mShadowSampler.Get() ) } );
    mShadowFrameGroup = CreateGroup( mGpu, "shadow frame", mShadowFrameLayout.Get(),
                                     vector<WGPUBindGroupEntry>{ BufferBinding( 0, mShadowFrame ) } );
    mObjectsGroup =
        CreateGroup( mGpu, "objects", mObjectsLayout.Get(),
                     vector<WGPUBindGroupEntry>{ BufferBinding( 0, mObjects ), BufferBinding( 1, mSkins ) } );
    mUiCanvasGroup = CreateGroup( mGpu, "ui canvases", mUiCanvasLayout.Get(),
                                  vector<WGPUBindGroupEntry>{ BufferBinding( 0, mUiCanvases ) } );
}

void Renderer::Resize( RenderView& view, ivec2 size )
{
    if ( view.mHdr and view.mHdr.Size() == size )
        return;
    view.mHdr = GpuTexture(
        mGpu, GpuTextureOptions{ .mSize = size,
                                 .mMips = 1,
                                 .mFormat = cHdrFormat,
                                 .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
                                 .mLabel = "hdr" } );
    view.mDepth = GpuTexture( mGpu, GpuTextureOptions{ .mSize = size,
                                                       .mMips = 1,
                                                       .mFormat = cDepthFormat,
                                                       .mUsage = WGPUTextureUsage_RenderAttachment,
                                                       .mLabel = "depth" } );
    view.mTonemapGroup = CreateGroup( mGpu, "tonemap", mTonemapLayout.Get(),
                                      vector<WGPUBindGroupEntry>{ TextureBinding( 0, view.mHdr.View() ) } );
}

WGPURenderPipeline Renderer::UiScreenFor( WGPUTextureFormat format )
{
    GpuRenderPipelineHandle& pipeline = mUiScreenPipelines[static_cast<u32>( format )];
    if ( not pipeline )
        pipeline = CreateRenderPipeline(
            mGpu, GpuPipelineOptions{ .mLabel = "ui on the screen",
                                      .mShader = mUi.Get(),
                                      .mVertexEntry = "vs_main",
                                      .mFragmentEntry = "fs_main",
                                      .mBindGroupLayouts = { mUiCanvasLayout.Get(), mUiImageLayout.Get() },
                                      .mVertexBuffers = { UiVertexLayout() },
                                      .mColorFormat = format,
                                      .mDepth = nullopt,
                                      .mTopology = WGPUPrimitiveTopology_TriangleList,
                                      .mCullMode = WGPUCullMode_None,
                                      .mBlend = true } );
    return pipeline.Get();
}

void Renderer::PrepareUi( const UiScene& ui, const mat4& viewProjection, ivec2 size )
{
    mUiVertexData.clear();
    mUiCanvasData.clear();
    mUiBatches.clear();
    mUiImageGroups.clear();
    for ( const UiCanvasView& canvas : ui.mCanvases )
    {
        if ( canvas.mWorld )
        {
            mUiCanvasData.push_back( viewProjection * canvas.mToWorld );
            continue;
        }
        // Its pixels across the target, Y down.
        mat4 toClip( 1.0f );
        toClip[0][0] = 2.0f / max( canvas.mSize.x, 1.0f );
        toClip[1][1] = -2.0f / max( canvas.mSize.y, 1.0f );
        toClip[3][0] = -1.0f;
        toClip[3][1] = 1.0f;
        mUiCanvasData.push_back( toClip );
    }
    (void)size;
    for ( const UiQuad& quad : ui.mQuads )
    {
        if ( quad.mCanvas >= ui.mCanvases.size() )
            continue;
        WGPUTextureView texture = mUiWhite.View();
        f32 range = 0;
        if ( quad.mKind == UiQuadKind::Glyph )
        {
            const auto atlasMaybe = mAssets.TryFont( quad.mFont );
            if ( not atlasMaybe )
                continue;
            texture = atlasMaybe->View();
            range = quad.mFont.TryGet()->mRange;
        }
        else if ( quad.mImage )
        {
            const auto imageMaybe = mAssets.TryTexture( quad.mImage );
            if ( not imageMaybe )
                continue;
            texture = imageMaybe->View();
        }
        const bool world = ui.mCanvases[quad.mCanvas].mWorld;
        if ( mUiBatches.empty() or mUiBatches.back().mTexture != texture or mUiBatches.back().mWorld != world )
            mUiBatches.push_back( UiBatch{ .mFirst = static_cast<u32>( mUiVertexData.size() ),
                                           .mCount = 0,
                                           .mTexture = texture,
                                           .mWorld = world } );
        if ( not mUiImageGroups.contains( texture ) )
            mUiImageGroups.emplace(
                texture, CreateGroup( mGpu, "ui image", mUiImageLayout.Get(),
                                      vector<WGPUBindGroupEntry>{ TextureBinding( 0, texture ),
                                                                  SamplerBinding( 1, mUiSampler.Get() ) } ) );
        const auto corner = [&]( f32 x, f32 y ) {
            return UiVertex{ .mPosition = vec2( x ? quad.mMax.x : quad.mMin.x, y ? quad.mMax.y : quad.mMin.y ),
                             .mUv = vec2( x ? quad.mUvMax.x : quad.mUvMin.x, y ? quad.mUvMax.y : quad.mUvMin.y ),
                             .mColor = quad.mColor,
                             .mCanvas = quad.mCanvas,
                             .mKind = quad.mKind == UiQuadKind::Glyph ? 1u : 0u,
                             .mRange = range,
                             .mPadding = 0 };
        };
        for ( const auto& [x, y] : { pair<f32, f32>( 0, 0 ), pair<f32, f32>( 1, 0 ), pair<f32, f32>( 1, 1 ),
                                     pair<f32, f32>( 0, 0 ), pair<f32, f32>( 1, 1 ), pair<f32, f32>( 0, 1 ) } )
            mUiVertexData.push_back( corner( x, y ) );
        mUiBatches.back().mCount += 6;
    }
}

void Renderer::DrawUi( WGPURenderPassEncoder pass, bool world, WGPURenderPipeline pipeline )
{
    bool bound = false;
    for ( const UiBatch& batch : mUiBatches )
    {
        if ( batch.mWorld != world )
            continue;
        if ( not bound )
        {
            wgpuRenderPassEncoderSetPipeline( pass, pipeline );
            wgpuRenderPassEncoderSetBindGroup( pass, 0, mUiCanvasGroup.Get(), 0, nullptr );
            wgpuRenderPassEncoderSetVertexBuffer( pass, 0, mUiVertices.Get(), 0, WGPU_WHOLE_SIZE );
            bound = true;
        }
        wgpuRenderPassEncoderSetBindGroup( pass, 1, mUiImageGroups.at( batch.mTexture ).Get(), 0, nullptr );
        wgpuRenderPassEncoderDraw( pass, batch.mCount, 1, batch.mFirst, 0 );
    }
}

WGPURenderPipeline Renderer::TonemapFor( WGPUTextureFormat format )
{
    GpuRenderPipelineHandle& pipeline = mTonemapPipelines[static_cast<u32>( format )];
    if ( not pipeline )
        pipeline = CreateRenderPipeline( mGpu, GpuPipelineOptions{ .mLabel = "tonemap",
                                                                   .mShader = mTonemap.Get(),
                                                                   .mVertexEntry = "vs_main",
                                                                   .mFragmentEntry = "fs_main",
                                                                   .mBindGroupLayouts = { mTonemapLayout.Get() },
                                                                   .mVertexBuffers = {},
                                                                   .mColorFormat = format,
                                                                   .mDepth = nullopt,
                                                                   .mTopology = WGPUPrimitiveTopology_TriangleList,
                                                                   .mCullMode = WGPUCullMode_None,
                                                                   .mBlend = false } );
    return pipeline.Get();
}

void Renderer::Collect( const RenderScene& scene )
{
    mItems.clear();
    mSkinData.clear();
    mBoundsMin = vec3( numeric_limits<f32>::max() );
    mBoundsMax = vec3( -numeric_limits<f32>::max() );
    for ( const RenderItem& source : scene.mItems )
    {
        const auto modelMaybe = mAssets.TryModel( source.mModel );
        if ( not modelMaybe )
            continue;
        const mat3 turn( source.mWorld );
        Item item{ .mSource = source,
                   .mModel = *modelMaybe,
                   .mObject = ObjectData{ .mModel = source.mWorld,
                                          .mNormal = mat4( glm::transpose( glm::inverse( turn ) ) ),
                                          .mTint = source.mTint,
                                          .mEntity = static_cast<u32>( source.mEntity ),
                                          .mSkin = 0,
                                          .mPadding = {} },
                   .mMin = vec3( numeric_limits<f32>::max() ),
                   .mMax = vec3( -numeric_limits<f32>::max() ),
                   .mCentre = vec3( 0.0f ),
                   .mRadius = 0,
                   .mScale = max( { glm::length( turn[0] ), glm::length( turn[1] ), glm::length( turn[2] ) } ) };
        // Its skin as posed, or at rest when the pose is not for this model.
        const vector<mat4>& rest = modelMaybe->RestSkin();
        if ( not rest.empty() )
        {
            item.mObject.mSkin = static_cast<u32>( mSkinData.size() );
            if ( source.mSkinCount == rest.size() )
                mSkinData.insert( mSkinData.end(), scene.mSkins.begin() + source.mSkinFirst,
                                  scene.mSkins.begin() + source.mSkinFirst + source.mSkinCount );
            else
                mSkinData.insert( mSkinData.end(), rest.begin(), rest.end() );
        }
        Grow( item.mMin, item.mMax, source.mWorld, modelMaybe->Min(), modelMaybe->Max() );
        item.mCentre = ( item.mMin + item.mMax ) * 0.5f;
        item.mRadius = glm::length( item.mMax - item.mMin ) * 0.5f;
        mBoundsMin = glm::min( mBoundsMin, item.mMin );
        mBoundsMax = glm::max( mBoundsMax, item.mMax );
        mItems.push_back( std::move( item ) );
    }
}

u32 Renderer::LodOf( RenderView& view, const Item& item, u32 mesh, vec3 eye, f32 pixelsAtOne )
{
    const vector<GpuLod>& lods = item.mModel.Meshes()[mesh].mLods;
    RenderView::LodChoice& choice = view.mLods[( u64{ static_cast<u32>( item.mSource.mEntity ) } << 16 ) | mesh];
    if ( choice.mFrame == view.mFrames )
        return choice.mLod;
    const u32 previous = choice.mFrame + 1 == view.mFrames ? choice.mLod : 0;
    // From the sphere's surface: inside it, or near, the full mesh.
    const f32 distance = max( glm::length( item.mCentre - eye ) - item.mRadius, 1e-3f );
    const f32 pixelsPerError = item.mScale * pixelsAtOne / distance;
    u32 lod = 0;
    for ( u32 level = static_cast<u32>( lods.size() ); level-- > 1; )
    {
        const f32 limit = mOptions.mLodPixels * ( level > previous ? 0.75f : 1.0f );
        if ( lods[level].mError * pixelsPerError <= limit )
        {
            lod = level;
            break;
        }
    }
    choice = RenderView::LodChoice{ .mLod = lod, .mFrame = view.mFrames };
    return lod;
}

WGPURenderPipeline Renderer::PipelineOf( const DrawCall& draw )
{
    return draw.mSkinned ? draw.mMaterial.mSkinnedPipeline : draw.mMaterial.mPipeline;
}

void Renderer::Order( const vector<DrawCall>& draws, vector<u32>& order, vector<Batch>& batches, bool shadow )
{
    order.resize( draws.size() );
    for ( u32 draw = 0; draw < draws.size(); ++draw )
        order[draw] = draw;
    const auto opaqueEnd = stable_partition( order.begin(), order.end(),
                                             [&]( u32 draw ) { return not draws[draw].mMaterial.mTransparent; } );
    // The shadow's pipeline is one a kind of mesh, which the mesh says; the
    // materials do not matter there.
    const auto key = [&]( const DrawCall& draw ) {
        return tuple<WGPURenderPipeline, const GpuMaterial*, const GpuModel*, u32, u32>(
            shadow ? nullptr : PipelineOf( draw ), shadow ? nullptr : &draw.mMaterial, &draw.mModel, draw.mMesh,
            draw.mLod );
    };
    sort( order.begin(), opaqueEnd, [&]( u32 a, u32 b ) { return key( draws[a] ) < key( draws[b] ); } );
    sort( opaqueEnd, order.end(), [&]( u32 a, u32 b ) { return draws[a].mDistance > draws[b].mDistance; } );

    batches.clear();
    const size_t opaque = static_cast<size_t>( opaqueEnd - order.begin() );
    for ( size_t at = 0; at < order.size(); ++at )
    {
        const DrawCall& draw = draws[order[at]];
        const bool joins =
            not batches.empty() and at < opaque and key( draws[order[batches.back().mDraw]] ) == key( draw );
        if ( joins )
            ++batches.back().mInstances;
        else
            batches.push_back(
                Batch{ .mDraw = at, .mFirstInstance = static_cast<u32>( mObjectData.size() ), .mInstances = 1 } );
        mObjectData.push_back( mItems[draw.mItem].mObject );
    }
}

void Renderer::Draw( RenderView& view,
                     const RenderScene& scene,
                     const RenderCamera& camera,
                     const RenderTarget& target,
                     OptRef<const UiScene> uiMaybe )
{
    BUBBLE_PROFILE_ZONE();
    if ( target.mSize.x <= 0 or target.mSize.y <= 0 )
        return;
    Resize( view, target.mSize );
    ++view.mFrames;
    mMaterials->BeginFrame();
    Collect( scene );

    const vec3 eye( camera.mWorld[3] );
    FrameUniforms frame{ .mViewProjection = ViewProjection( camera, target.mSize ),
                         .mShadowViewProjection = mat4( 1.0f ),
                         .mCameraPosition = eye,
                         .mLightCount = static_cast<u32>( scene.mLights.size() ),
                         .mAmbient = vec3( ToLinear( mOptions.mAmbient ) ) * mOptions.mAmbientIntensity,
                         .mShadowLight = cNoShadow,
                         .mShadowNormalOffset = 0,
                         .mTime = scene.mTime,
                         .mPadding = {} };
    // The first directional light that casts shadows, when there is
    // anything to cast them.
    if ( not mItems.empty() )
        for ( u32 light = 0; light < scene.mLights.size(); ++light )
            if ( scene.mLights[light].mKind == RenderLightKind::Directional and scene.mLights[light].mShadows )
            {
                f32 texelSize = 0;
                frame.mShadowViewProjection = ShadowViewProjection( scene.mLights[light].mDirection, mBoundsMin,
                                                                    mBoundsMax, texelSize, mOptions.mShadowMapSize );
                frame.mShadowLight = light;
                frame.mShadowNormalOffset = texelSize * 1.5f;
                break;
            }

    // What the camera sees, each mesh at its level; what the light sees, a
    // level coarser - a shadow's edge hides more than a surface does.
    const Frustum seen = FrustumOf( frame.mViewProjection );
    const Frustum light = FrustumOf( frame.mShadowViewProjection );
    const f32 pixelsAtOne = static_cast<f32>( target.mSize.y ) / ( 2.0f * tan( camera.mFov * 0.5f ) );
    mDraws.clear();
    mShadowDraws.clear();
    view.mStats = RenderStats();
    view.mStats.mItems = static_cast<u32>( mItems.size() );
    for ( u32 at = 0; at < mItems.size(); ++at )
    {
        const Item& item = mItems[at];
        const bool inView = Overlaps( seen, item.mMin, item.mMax );
        const bool casts = frame.mShadowLight != cNoShadow and Overlaps( light, item.mMin, item.mMax );
        if ( not inView and not casts )
            continue;
        view.mStats.mVisible += inView ? 1 : 0;
        const vec3 toItem = item.mCentre - eye;
        const vector<GpuMesh>& meshes = item.mModel.Meshes();
        for ( u32 mesh = 0; mesh < meshes.size(); ++mesh )
        {
            const AssetRef<MaterialAsset> none;
            const auto& materials = item.mSource.mMaterials;
            const GpuMaterial& material = mMaterials->For( mesh < materials.size() ? materials[mesh] : none );
            const u32 lod = LodOf( view, item, mesh, eye, pixelsAtOne );
            const DrawCall draw{ .mModel = item.mModel,
                                 .mMaterial = material,
                                 .mMesh = mesh,
                                 .mLod = lod,
                                 .mItem = at,
                                 .mDistance = glm::dot( toItem, toItem ),
                                 .mSkinned = meshes[mesh].mSkinned };
            if ( inView )
                mDraws.push_back( draw );
            // What is transparent casts no shadow.
            if ( casts and not material.mTransparent )
            {
                mShadowDraws.push_back( draw );
                mShadowDraws.back().mLod = min( lod + 1, static_cast<u32>( meshes[mesh].mLods.size() - 1 ) );
            }
        }
    }
    // The debug drawing's lines, in linear light.
    mLineData.clear();
    for ( const DebugLine& line : scene.mLines )
    {
        const vec4 color = ToLinear( line.mColor );
        mLineData.push_back( LineVertex{ .mPosition = line.mFrom, .mColor = color } );
        mLineData.push_back( LineVertex{ .mPosition = line.mTo, .mColor = color } );
    }
    if ( not mLineData.empty() )
    {
        Fit( mLineVertices, mLineData.size() * sizeof( LineVertex ), "debug lines", WGPUBufferUsage_Vertex );
        mLineVertices.Write( mGpu, 0, AsBytes( mLineData ) );
    }

    // Choices for meshes no longer drawn go.
    erase_if( view.mLods, [&]( const auto& choice ) { return choice.second.mFrame + 1 < view.mFrames; } );

    mObjectData.clear();
    Order( mDraws, mOrder, mBatches, false );
    Order( mShadowDraws, mShadowOrder, mShadowBatches, true );
    view.mStats.mDrawCalls = static_cast<u32>( mBatches.size() );
    view.mStats.mShadowDrawCalls = static_cast<u32>( mShadowBatches.size() );
    for ( const Batch& batch : mBatches )
    {
        const DrawCall& draw = mDraws[mOrder[batch.mDraw]];
        view.mStats.mInstances += batch.mInstances;
        view.mStats.mTriangles +=
            u64{ draw.mModel.Meshes()[draw.mMesh].mLods[draw.mLod].mIndexCount / 3 } * batch.mInstances;
    }

    mLightData.clear();
    for ( const RenderLight& sceneLight : scene.mLights )
        mLightData.push_back( LightData{ .mPosition = sceneLight.mPosition,
                                         .mRange = sceneLight.mRange,
                                         .mDirection = sceneLight.mDirection,
                                         .mInnerCos = sceneLight.mInnerCos,
                                         .mRadiance = sceneLight.mRadiance,
                                         .mOuterCos = sceneLight.mOuterCos,
                                         .mDirectional = sceneLight.mKind == RenderLightKind::Directional ? 1u : 0u } );

    bool grown = Fit( mLights, max<u64>( mLightData.size(), 1 ) * sizeof( LightData ), "lights" );
    grown = Fit( mObjects, max<u64>( mObjectData.size(), 1 ) * sizeof( ObjectData ), "objects" ) or grown;
    grown = Fit( mSkins, max<u64>( mSkinData.size(), 1 ) * sizeof( mat4 ), "skins" ) or grown;
    mUiBatches.clear();
    if ( uiMaybe )
    {
        PrepareUi( *uiMaybe, frame.mViewProjection, target.mSize );
        grown = Fit( mUiCanvases, max<u64>( mUiCanvasData.size(), 1 ) * sizeof( mat4 ), "ui canvases" ) or grown;
        Fit( mUiVertices, max<u64>( mUiVertexData.size(), 1 ) * sizeof( UiVertex ), "ui vertices",
             WGPUBufferUsage_Vertex );
    }
    if ( grown )
        MakeBindGroups();
    mFrame.Write( mGpu, 0, AsBytes( frame ) );
    mShadowFrame.Write( mGpu, 0, AsBytes( frame.mShadowViewProjection ) );
    if ( not mLightData.empty() )
        mLights.Write( mGpu, 0, AsBytes( mLightData ) );
    if ( not mObjectData.empty() )
        mObjects.Write( mGpu, 0, AsBytes( mObjectData ) );
    if ( not mSkinData.empty() )
        mSkins.Write( mGpu, 0, AsBytes( mSkinData ) );
    if ( not mUiCanvasData.empty() and uiMaybe )
        mUiCanvases.Write( mGpu, 0, AsBytes( mUiCanvasData ) );
    if ( not mUiVertexData.empty() and uiMaybe )
        mUiVertices.Write( mGpu, 0, AsBytes( mUiVertexData ) );

    const GpuCommandEncoderHandle encoder( wgpuDeviceCreateCommandEncoder( mGpu.Device(), nullptr ) );
    // The calls; with their materials, or - the shadow's - with the
    // pipeline already set.
    const auto drawBatches = [&]( WGPURenderPassEncoder pass, const vector<DrawCall>& draws, const vector<u32>& order,
                                  const vector<Batch>& batches, bool materials ) {
        OptRef<const GpuModel> boundModel;
        OptRef<const GpuMaterial> boundMaterial;
        WGPURenderPipeline boundPipeline = nullptr;
        for ( const Batch& batch : batches )
        {
            const DrawCall& draw = draws[order[batch.mDraw]];
            const WGPURenderPipeline pipeline =
                materials ? PipelineOf( draw ) : mShadowPipelines[draw.mSkinned ? 1 : 0].Get();
            if ( boundPipeline != pipeline )
            {
                wgpuRenderPassEncoderSetPipeline( pass, pipeline );
                boundPipeline = pipeline;
            }
            if ( materials and boundMaterial != OptRef<const GpuMaterial>( draw.mMaterial ) )
            {
                wgpuRenderPassEncoderSetBindGroup( pass, 2, draw.mMaterial.mGroup.Get(), 0, nullptr );
                boundMaterial = draw.mMaterial;
            }
            if ( boundModel != OptRef<const GpuModel>( draw.mModel ) )
            {
                draw.mModel.Bind( pass );
                boundModel = draw.mModel;
            }
            draw.mModel.Draw( pass, draw.mMesh, draw.mLod, batch.mFirstInstance, batch.mInstances );
        }
    };

    if ( frame.mShadowLight != cNoShadow )
    {
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = mShadowMap.View();
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Store;
        depth.depthClearValue = 0.0f;
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "shadow" );
        descriptor.depthStencilAttachment = &depth;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mShadowFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawBatches( pass.Get(), mShadowDraws, mShadowOrder, mShadowBatches, false );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }

    {
        const vec4 background = ToLinear( view.mBackgroundMaybe.value_or( mOptions.mBackground ) );
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = view.mHdr.View();
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        color.clearValue = WGPUColor{ background.r, background.g, background.b, 1.0 };
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = view.mDepth.View();
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Discard;
        depth.depthClearValue = 0.0f;
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "surfaces" );
        descriptor.colorAttachmentCount = 1;
        descriptor.colorAttachments = &color;
        descriptor.depthStencilAttachment = &depth;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawBatches( pass.Get(), mDraws, mOrder, mBatches, true );
        if ( not mLineData.empty() )
        {
            wgpuRenderPassEncoderSetPipeline( pass.Get(), mLinePipeline.Get() );
            wgpuRenderPassEncoderSetVertexBuffer( pass.Get(), 0, mLineVertices.Get(), 0, WGPU_WHOLE_SIZE );
            wgpuRenderPassEncoderDraw( pass.Get(), static_cast<u32>( mLineData.size() ), 1, 0, 0 );
        }
        DrawUi( pass.Get(), true, mUiWorldPipeline.Get() );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }

    const bool picking = view.mPickWanted.has_value();
    if ( picking )
        EncodePick( view, encoder.Get(), target.mSize );

    {
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = target.mView;
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        color.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 };
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "tonemap" );
        descriptor.colorAttachmentCount = 1;
        descriptor.colorAttachments = &color;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
        wgpuRenderPassEncoderSetPipeline( pass.Get(), TonemapFor( target.mFormat ) );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, view.mTonemapGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderDraw( pass.Get(), 3, 1, 0, 0 );
        // The screen's UI over the picture, in its own colors.
        DrawUi( pass.Get(), false, UiScreenFor( target.mFormat ) );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }
    mGpu.Submit( encoder );

    // Mapped once the GPU has done what was submitted; the answer comes at
    // a Poll.
    if ( picking )
    {
        view.mPicking = CreateRef<RenderView::PickState>();
        WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowProcessEvents;
        callback.callback = &OnPickMapped;
        callback.userdata1 = CreateScope<Ref<RenderView::PickState>>( view.mPicking ).release();
        wgpuBufferMapAsync( view.mPickBuffer.Get(), WGPUMapMode_Read, 0, static_cast<size_t>( cPickBytes ), callback );
    }
}
}
