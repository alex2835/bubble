#include "bubble/render/gpu/renderer.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "gpu_binding.hpp"
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
};
static_assert( sizeof( Renderer::ObjectData ) == 144 );

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

    const WGPUShaderStage both = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    mFrameLayout = CreateLayout( gpu, "frame",
                                 vector<WGPUBindGroupLayoutEntry>{
                                     BufferEntry( 0, both, WGPUBufferBindingType_Uniform ),
                                     BufferEntry( 1, WGPUShaderStage_Fragment, WGPUBufferBindingType_ReadOnlyStorage ),
                                     TextureEntry( 2, WGPUTextureSampleType_Depth ),
                                     SamplerEntry( 3, WGPUSamplerBindingType_Comparison ),
                                 } );
    // The vertex stage reads an object's matrices, the fragment its tint.
    mObjectsLayout = CreateLayout(
        gpu, "objects",
        vector<WGPUBindGroupLayoutEntry>{ BufferEntry( 0, both, WGPUBufferBindingType_ReadOnlyStorage ) } );
    mShadowFrameLayout = CreateLayout(
        gpu, "shadow frame",
        vector<WGPUBindGroupLayoutEntry>{ BufferEntry( 0, WGPUShaderStage_Vertex, WGPUBufferBindingType_Uniform ) } );
    mTonemapLayout = CreateLayout( gpu, "tonemap",
                                   vector<WGPUBindGroupLayoutEntry>{ TextureEntry( 0, WGPUTextureSampleType_Float ) } );

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
                                 // Both sides cast: a two-sided leaf as much
                                 // as a closed box, whose far side the
                                 // near one hides from the light anyway.
                                 .mCullMode = WGPUCullMode_None,
                                 .mBlend = false } );

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
    mMaterials = CreateScope<GpuMaterials>( gpu, assets, mAssets, mFrameLayout.Get(), mObjectsLayout.Get() );
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
                                                                   .mCullMode = WGPUCullMode_None,
                                                                   .mBlend = false } );
    return pipeline.Get();
}

void Renderer::Collect( const RenderScene& scene )
{
    mItems.clear();
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
                                          .mTint = source.mTint },
                   .mMin = vec3( numeric_limits<f32>::max() ),
                   .mMax = vec3( -numeric_limits<f32>::max() ),
                   .mCentre = vec3( 0.0f ),
                   .mRadius = 0,
                   .mScale = max( { glm::length( turn[0] ), glm::length( turn[1] ), glm::length( turn[2] ) } ) };
        Grow( item.mMin, item.mMax, source.mWorld, modelMaybe->Min(), modelMaybe->Max() );
        item.mCentre = ( item.mMin + item.mMax ) * 0.5f;
        item.mRadius = glm::length( item.mMax - item.mMin ) * 0.5f;
        mBoundsMin = glm::min( mBoundsMin, item.mMin );
        mBoundsMax = glm::max( mBoundsMax, item.mMax );
        mItems.push_back( std::move( item ) );
    }
}

u32 Renderer::LodOf( const Item& item, u32 mesh, vec3 eye, f32 pixelsAtOne )
{
    const vector<GpuLod>& lods = item.mModel.Meshes()[mesh].mLods;
    LodChoice& choice = mLods[( u64{ static_cast<u32>( item.mSource.mEntity ) } << 16 ) | mesh];
    if ( choice.mFrame == mFrameCount )
        return choice.mLod;
    const u32 previous = choice.mFrame + 1 == mFrameCount ? choice.mLod : 0;
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
    choice = LodChoice{ .mLod = lod, .mFrame = mFrameCount };
    return lod;
}

void Renderer::Order( const vector<DrawCall>& draws, vector<u32>& order, vector<Batch>& batches, bool shadow )
{
    order.resize( draws.size() );
    for ( u32 draw = 0; draw < draws.size(); ++draw )
        order[draw] = draw;
    const auto opaqueEnd = stable_partition( order.begin(), order.end(),
                                             [&]( u32 draw ) { return not draws[draw].mMaterial.mTransparent; } );
    // The shadow's pipeline is one; the materials do not matter there.
    const auto key = [&]( const DrawCall& draw ) {
        return tuple<WGPURenderPipeline, const GpuMaterial*, const GpuModel*, u32, u32>(
            shadow ? nullptr : draw.mMaterial.mPipeline, shadow ? nullptr : &draw.mMaterial, &draw.mModel, draw.mMesh,
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

void Renderer::Draw( const RenderScene& scene, const RenderCamera& camera, const RenderTarget& target )
{
    BUBBLE_PROFILE_ZONE();
    if ( target.mSize.x <= 0 or target.mSize.y <= 0 )
        return;
    Resize( target.mSize );
    ++mFrameCount;
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
    const Frustum view = FrustumOf( frame.mViewProjection );
    const Frustum light = FrustumOf( frame.mShadowViewProjection );
    const f32 pixelsAtOne = static_cast<f32>( target.mSize.y ) / ( 2.0f * tan( camera.mFov * 0.5f ) );
    mDraws.clear();
    mShadowDraws.clear();
    mStats = RenderStats();
    mStats.mItems = static_cast<u32>( mItems.size() );
    for ( u32 at = 0; at < mItems.size(); ++at )
    {
        const Item& item = mItems[at];
        const bool seen = Overlaps( view, item.mMin, item.mMax );
        const bool casts = frame.mShadowLight != cNoShadow and Overlaps( light, item.mMin, item.mMax );
        if ( not seen and not casts )
            continue;
        mStats.mVisible += seen ? 1 : 0;
        const vec3 toItem = item.mCentre - eye;
        const vector<GpuMesh>& meshes = item.mModel.Meshes();
        for ( u32 mesh = 0; mesh < meshes.size(); ++mesh )
        {
            const AssetRef<MaterialAsset> none;
            const auto& materials = item.mSource.mMaterials;
            const GpuMaterial& material = mMaterials->For( mesh < materials.size() ? materials[mesh] : none );
            const u32 lod = LodOf( item, mesh, eye, pixelsAtOne );
            const DrawCall draw{ .mModel = item.mModel,
                                 .mMaterial = material,
                                 .mMesh = mesh,
                                 .mLod = lod,
                                 .mItem = at,
                                 .mDistance = glm::dot( toItem, toItem ) };
            if ( seen )
                mDraws.push_back( draw );
            // What is transparent casts no shadow.
            if ( casts and not material.mTransparent )
            {
                mShadowDraws.push_back( draw );
                mShadowDraws.back().mLod = min( lod + 1, static_cast<u32>( meshes[mesh].mLods.size() - 1 ) );
            }
        }
    }
    // Choices for meshes no longer drawn go.
    erase_if( mLods, [&]( const auto& choice ) { return choice.second.mFrame + 1 < mFrameCount; } );

    mObjectData.clear();
    Order( mDraws, mOrder, mBatches, false );
    Order( mShadowDraws, mShadowOrder, mShadowBatches, true );
    mStats.mDrawCalls = static_cast<u32>( mBatches.size() );
    mStats.mShadowDrawCalls = static_cast<u32>( mShadowBatches.size() );
    for ( const Batch& batch : mBatches )
    {
        const DrawCall& draw = mDraws[mOrder[batch.mDraw]];
        mStats.mInstances += batch.mInstances;
        mStats.mTriangles +=
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
    if ( grown )
        MakeBindGroups();
    mFrame.Write( mGpu, 0, AsBytes( frame ) );
    mShadowFrame.Write( mGpu, 0, AsBytes( frame.mShadowViewProjection ) );
    if ( not mLightData.empty() )
        mLights.Write( mGpu, 0, AsBytes( mLightData ) );
    if ( not mObjectData.empty() )
        mObjects.Write( mGpu, 0, AsBytes( mObjectData ) );

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
            if ( materials and boundMaterial != OptRef<const GpuMaterial>( draw.mMaterial ) )
            {
                if ( boundPipeline != draw.mMaterial.mPipeline )
                {
                    wgpuRenderPassEncoderSetPipeline( pass, draw.mMaterial.mPipeline );
                    boundPipeline = draw.mMaterial.mPipeline;
                }
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
        wgpuRenderPassEncoderSetPipeline( pass.Get(), mShadowPipeline.Get() );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mShadowFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawBatches( pass.Get(), mShadowDraws, mShadowOrder, mShadowBatches, false );
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
        descriptor.label = ToGpuString( "surfaces" );
        descriptor.colorAttachmentCount = 1;
        descriptor.colorAttachments = &color;
        descriptor.depthStencilAttachment = &depth;
        const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mFrameGroup.Get(), 0, nullptr );
        wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, mObjectsGroup.Get(), 0, nullptr );
        drawBatches( pass.Get(), mDraws, mOrder, mBatches, true );
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
