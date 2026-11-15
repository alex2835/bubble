#include "bubble/render/gpu/renderer.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
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
    array<f32, 3> mPadding{};
};
static_assert( sizeof( Renderer::FrameUniforms ) == 176 );
static_assert( offsetof( Renderer::FrameUniforms, mCameraPosition ) == 128 );
static_assert( offsetof( Renderer::FrameUniforms, mAmbient ) == 144 );
static_assert( offsetof( Renderer::FrameUniforms, mShadowNormalOffset ) == 160 );

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
};
static_assert( sizeof( Renderer::ObjectData ) == 128 );

namespace
{
constexpr WGPUTextureFormat cHdrFormat = WGPUTextureFormat_RGBA16Float;
constexpr WGPUTextureFormat cDepthFormat = WGPUTextureFormat_Depth32Float;
// No light has it: no shadow this frame.
constexpr u32 cNoShadow = ~0u;

WGPUBindGroupLayoutEntry BufferEntry( u32 binding, WGPUShaderStage visibility, WGPUBufferBindingType type )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = visibility;
    entry.buffer.type = type;
    return entry;
}

WGPUBindGroupLayoutEntry TextureEntry( u32 binding, WGPUTextureSampleType sampleType )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.texture.sampleType = sampleType;
    entry.texture.viewDimension = WGPUTextureViewDimension_2D;
    return entry;
}

WGPUBindGroupLayoutEntry ComparisonSamplerEntry( u32 binding )
{
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = binding;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.sampler.type = WGPUSamplerBindingType_Comparison;
    return entry;
}

GpuBindGroupLayoutHandle CreateLayout( const Gpu& gpu,
                                       string_view label,
                                       const vector<WGPUBindGroupLayoutEntry>& entries )
{
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( label );
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return GpuBindGroupLayoutHandle( wgpuDeviceCreateBindGroupLayout( gpu.Device(), &descriptor ) );
}

WGPUBindGroupEntry BufferBinding( u32 binding, const GpuBuffer& buffer )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.buffer = buffer.Get();
    entry.size = buffer.Size();
    return entry;
}

WGPUBindGroupEntry TextureBinding( u32 binding, WGPUTextureView view )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.textureView = view;
    return entry;
}

WGPUBindGroupEntry SamplerBinding( u32 binding, WGPUSampler sampler )
{
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = binding;
    entry.sampler = sampler;
    return entry;
}

GpuBindGroupHandle CreateGroup( const Gpu& gpu,
                                string_view label,
                                WGPUBindGroupLayout layout,
                                const vector<WGPUBindGroupEntry>& entries )
{
    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( label );
    descriptor.layout = layout;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return GpuBindGroupHandle( wgpuDeviceCreateBindGroup( gpu.Device(), &descriptor ) );
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

Renderer::Renderer( Gpu& gpu, const AssetRegistry& assets, RendererOptions options )
    : mGpu( gpu ),
      mAssets( gpu, assets ),
      mOptions( options )
{
    mStandard = CreateShader( gpu, EngineShader( "standard.wgsl" ), "standard.wgsl" );
    mShadow = CreateShader( gpu, EngineShader( "shadow.wgsl" ), "shadow.wgsl" );
    mTonemap = CreateShader( gpu, EngineShader( "tonemap.wgsl" ), "tonemap.wgsl" );

    const WGPUShaderStage both = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    mFrameLayout = CreateLayout( gpu, "frame",
                                 vector<WGPUBindGroupLayoutEntry>{
                                     BufferEntry( 0, both, WGPUBufferBindingType_Uniform ),
                                     BufferEntry( 1, WGPUShaderStage_Fragment, WGPUBufferBindingType_ReadOnlyStorage ),
                                     TextureEntry( 2, WGPUTextureSampleType_Depth ),
                                     ComparisonSamplerEntry( 3 ),
                                 } );
    mObjectsLayout = CreateLayout( gpu, "objects",
                                   vector<WGPUBindGroupLayoutEntry>{ BufferEntry(
                                       0, WGPUShaderStage_Vertex, WGPUBufferBindingType_ReadOnlyStorage ) } );
    mShadowFrameLayout = CreateLayout(
        gpu, "shadow frame",
        vector<WGPUBindGroupLayoutEntry>{ BufferEntry( 0, WGPUShaderStage_Vertex, WGPUBufferBindingType_Uniform ) } );
    mTonemapLayout = CreateLayout( gpu, "tonemap",
                                   vector<WGPUBindGroupLayoutEntry>{ TextureEntry( 0, WGPUTextureSampleType_Float ) } );

    mStandardPipeline = CreateRenderPipeline(
        gpu, GpuPipelineOptions{ .mLabel = "standard",
                                 .mShader = mStandard.Get(),
                                 .mVertexEntry = "vs_main",
                                 .mFragmentEntry = "fs_main",
                                 .mBindGroupLayouts = { mFrameLayout.Get(), mObjectsLayout.Get() },
                                 .mVertexBuffers = { ModelVertexLayout() },
                                 .mColorFormat = cHdrFormat,
                                 .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat },
                                 .mTopology = WGPUPrimitiveTopology_TriangleList,
                                 .mCullMode = WGPUCullMode_Back } );
    // Depth is reversed: pushing a caster away from the light is a bias
    // down.
    mShadowPipeline = CreateRenderPipeline(
        gpu, GpuPipelineOptions{ .mLabel = "shadow",
                                 .mShader = mShadow.Get(),
                                 .mVertexEntry = "vs_main",
                                 .mFragmentEntry = "",
                                 .mBindGroupLayouts = { mShadowFrameLayout.Get(), mObjectsLayout.Get() },
                                 .mVertexBuffers = { ModelVertexLayout() },
                                 .mColorFormat = WGPUTextureFormat_Undefined,
                                 .mDepth = GpuDepthOptions{ .mFormat = cDepthFormat,
                                                            .mCompare = WGPUCompareFunction_Greater,
                                                            .mWrite = true,
                                                            .mBias = -2,
                                                            .mBiasSlopeScale = -2.0f },
                                 .mTopology = WGPUPrimitiveTopology_TriangleList,
                                 .mCullMode = WGPUCullMode_Back } );

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
    MakeBindGroups();
}

Renderer::~Renderer() = default;

bool Renderer::Fit( GpuBuffer& buffer, u64 bytes, string_view label )
{
    if ( buffer and buffer.Size() >= bytes )
        return false;
    // Twice as much as asked: a scene that grows a little does not make it
    // again every frame.
    buffer = GpuBuffer( mGpu, WGPUBufferUsage_Storage, max( bytes * 2, buffer.Size() * 2 ), label );
    return true;
}

void Renderer::MakeBindGroups()
{
    mFrameGroup = CreateGroup( mGpu, "frame", mFrameLayout.Get(),
                               vector<WGPUBindGroupEntry>{ BufferBinding( 0, mFrame ), BufferBinding( 1, mLights ),
                                                           TextureBinding( 2, mShadowMap.View() ),
                                                           SamplerBinding( 3, mShadowSampler.Get() ) } );
    mShadowFrameGroup = CreateGroup( mGpu, "shadow frame", mShadowFrameLayout.Get(),
                                     vector<WGPUBindGroupEntry>{ BufferBinding( 0, mShadowFrame ) } );
    mObjectsGroup = CreateGroup( mGpu, "objects", mObjectsLayout.Get(),
                                 vector<WGPUBindGroupEntry>{ BufferBinding( 0, mObjects ) } );
}

void Renderer::Resize( ivec2 size )
{
    if ( mHdr and mHdr.Size() == size )
        return;
    mHdr = GpuTexture( mGpu,
                       GpuTextureOptions{ .mSize = size,
                                          .mMips = 1,
                                          .mFormat = cHdrFormat,
                                          .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
                                          .mLabel = "hdr" } );
    mDepth = GpuTexture( mGpu, GpuTextureOptions{ .mSize = size,
                                                  .mMips = 1,
                                                  .mFormat = cDepthFormat,
                                                  .mUsage = WGPUTextureUsage_RenderAttachment,
                                                  .mLabel = "depth" } );
    mTonemapGroup = CreateGroup( mGpu, "tonemap", mTonemapLayout.Get(),
                                 vector<WGPUBindGroupEntry>{ TextureBinding( 0, mHdr.View() ) } );
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
                                                                   .mCullMode = WGPUCullMode_None } );
    return pipeline.Get();
}

void Renderer::Gather( const RenderScene& scene )
{
    mObjectData.clear();
    mDraws.clear();
    mBoundsMin = vec3( numeric_limits<f32>::max() );
    mBoundsMax = vec3( -numeric_limits<f32>::max() );
    for ( const RenderItem& item : scene.mItems )
    {
        const auto modelMaybe = mAssets.TryModel( item.mModel );
        if ( not modelMaybe )
            continue;
        const u32 object = static_cast<u32>( mObjectData.size() );
        mObjectData.push_back( ObjectData{ .mModel = item.mWorld,
                                           .mNormal = mat4( glm::transpose( glm::inverse( mat3( item.mWorld ) ) ) ) } );
        for ( u32 mesh = 0; mesh < modelMaybe->Meshes().size(); ++mesh )
            mDraws.push_back( DrawCall{ .mModel = *modelMaybe, .mMesh = mesh, .mObject = object } );
        Grow( mBoundsMin, mBoundsMax, item.mWorld, modelMaybe->Min(), modelMaybe->Max() );
    }

    mLightData.clear();
    for ( const RenderLight& light : scene.mLights )
        mLightData.push_back( LightData{ .mPosition = light.mPosition,
                                         .mRange = light.mRange,
                                         .mDirection = light.mDirection,
                                         .mInnerCos = light.mInnerCos,
                                         .mRadiance = light.mRadiance,
                                         .mOuterCos = light.mOuterCos,
                                         .mDirectional = light.mKind == RenderLightKind::Directional ? 1u : 0u } );
}

void Renderer::Draw( const RenderScene& scene, const RenderCamera& camera, const RenderTarget& target )
{
    BUBBLE_PROFILE_ZONE();
    if ( target.mSize.x <= 0 or target.mSize.y <= 0 )
        return;
    Resize( target.mSize );
    Gather( scene );

    FrameUniforms frame{ .mViewProjection = ViewProjection( camera, target.mSize ),
                         .mShadowViewProjection = mat4( 1.0f ),
                         .mCameraPosition = vec3( camera.mWorld[3] ),
                         .mLightCount = static_cast<u32>( mLightData.size() ),
                         .mAmbient = vec3( ToLinear( mOptions.mAmbient ) ) * mOptions.mAmbientIntensity,
                         .mShadowLight = cNoShadow,
                         .mShadowNormalOffset = 0,
                         .mPadding = {} };
    // The first directional light that casts shadows, when there is
    // anything to cast them.
    if ( not mObjectData.empty() )
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

    bool grown = Fit( mLights, max<u64>( mLightData.size(), 1 ) * sizeof( LightData ), "lights" );
    grown = Fit( mObjects, max<u64>( mObjectData.size(), 1 ) * sizeof( ObjectData ), "objects" ) or grown;
    if ( grown )
        MakeBindGroups();
    mFrame.Write( mGpu, 0, AsBytes( frame ) );
    mShadowFrame.Write( mGpu, 0, AsBytes( frame.mShadowViewProjection ) );
    if ( not mLightData.empty() )
        mLights.Write( mGpu, 0, AsBytes( mLightData ) );
    if ( not mObjectData.empty() )
        mObjects.Write( mGpu, 0, AsBytes( mObjectData ) );

    const GpuCommandEncoderHandle encoder( wgpuDeviceCreateCommandEncoder( mGpu.Device(), nullptr ) );
    const auto drawAll = [&]( WGPURenderPassEncoder pass ) {
        OptRef<const GpuModel> bound;
        for ( const DrawCall& draw : mDraws )
        {
            if ( bound != OptRef<const GpuModel>( draw.mModel ) )
            {
                draw.mModel.Bind( pass );
                bound = draw.mModel;
            }
            draw.mModel.Draw( pass, draw.mMesh, 0, draw.mObject );
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
        wgpuRenderPassEncoderSetPipeline( pass.Get(), mShadowPipeline.Get() );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mShadowFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawAll( pass.Get() );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }

    {
        const vec4 background = ToLinear( mOptions.mBackground );
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = mHdr.View();
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        color.clearValue = WGPUColor{ background.r, background.g, background.b, 1.0 };
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = mDepth.View();
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Discard;
        depth.depthClearValue = 0.0f;
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.label = ToGpuString( "opaque" );
        descriptor.colorAttachmentCount = 1;
        descriptor.colorAttachments = &color;
        descriptor.depthStencilAttachment = &depth;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
        wgpuRenderPassEncoderSetPipeline( pass.Get(), mStandardPipeline.Get() );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawAll( pass.Get() );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }

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
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mTonemapGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderDraw( pass.Get(), 3, 1, 0, 0 );
        wgpuRenderPassEncoderEnd( pass.Get() );
    }
    mGpu.Submit( encoder );
}
}
