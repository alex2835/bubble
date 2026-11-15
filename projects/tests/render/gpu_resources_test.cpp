// GPU resources: buffers, a model and an image made from assets, a WGSL
// pipeline drawing them into a texture whose pixels come back, and the
// GPU's checks of what it was given. Not on the web, as gpu_test.cpp.
#ifndef __EMSCRIPTEN__
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/gpu_buffer.hpp"
#include "bubble/render/gpu/gpu_model.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/render/gpu/gpu_texture.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
// A model as it is - its positions in clip space already - in the image's
// colors times a tint.
constexpr string_view cShader = R"wgsl(
struct Vertex
{
    @location( 0 ) position: vec3f,
    @location( 1 ) normal: vec3f,
    @location( 2 ) uv: vec2f,
}

struct Fragment
{
    @builtin( position ) position: vec4f,
    @location( 0 ) uv: vec2f,
}

@group( 0 ) @binding( 0 ) var image: texture_2d<f32>;
@group( 0 ) @binding( 1 ) var nearest: sampler;
@group( 0 ) @binding( 2 ) var<uniform> tint: vec4f;

@vertex
fn vs_main( vertex: Vertex ) -> Fragment
{
    return Fragment( vec4f( vertex.position, 1.0 ), vertex.uv );
}

@fragment
fn fs_main( fragment: Fragment ) -> @location( 0 ) vec4f
{
    return textureSample( image, nearest, fragment.uv ) * tint;
}

// The image's second level, whatever its size on screen.
@fragment
fn fs_second_level( fragment: Fragment ) -> @location( 0 ) vec4f
{
    return textureSampleLevel( image, nearest, fragment.uv, 1.0 ) * tint;
}
)wgsl";

// The screen's corners, the image's top left at the top left: the whole
// quad, then the triangle below its diagonal - down to the right.
ModelMesh ScreenQuad()
{
    ModelMesh mesh;
    mesh.mMaterial = "screen";
    mesh.mVertices = vector<ModelVertex>{
        ModelVertex{ .mPosition = { -1.0f, -1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 0.0f, 1.0f } },
        ModelVertex{ .mPosition = { 1.0f, -1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 1.0f, 1.0f } },
        ModelVertex{ .mPosition = { 1.0f, 1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 1.0f, 0.0f } },
        ModelVertex{ .mPosition = { -1.0f, 1.0f, 0.0f }, .mNormal = { 0.0f, 0.0f, 1.0f }, .mUv = { 0.0f, 0.0f } },
    };
    mesh.mLods = vector<ModelLod>{
        ModelLod{ .mError = 0.0f, .mIndices = { 0, 1, 2, 0, 2, 3 } },
        ModelLod{ .mError = 0.5f, .mIndices = { 0, 1, 2 } },
    };
    return mesh;
}

// A triangle first, so the quad's vertices and indices start past it.
ModelAsset Model()
{
    ModelMesh first;
    first.mMaterial = "first";
    first.mVertices = vector<ModelVertex>( 3 );
    first.mLods = vector<ModelLod>{ ModelLod{ .mError = 0.0f, .mIndices = { 0, 1, 2 } } };
    return ModelAsset{ .mMin = { -1.0f, -1.0f, 0.0f },
                       .mMax = { 1.0f, 1.0f, 0.0f },
                       .mMeshes = vector<ModelMesh>{ first, ScreenQuad() } };
}

// Red and green above blue and white; the second level one grey-blue texel.
TextureAsset Image()
{
    return TextureAsset{ .mSrgb = true,
                         .mMips = vector<TextureMip>{
                             TextureMip{
                                 .mWidth = 2,
                                 .mHeight = 2,
                                 .mPixels = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255 } },
                             TextureMip{ .mWidth = 1, .mHeight = 1, .mPixels = { 64, 128, 192, 255 } },
                         } };
}

constexpr i32 cTargetSize = 4;

// The model, the image, and a 4×4 target the model is drawn into, its quad
// filling it.
struct Drawing
{
    Scope<Gpu> mGpu = StartGpu();
    GpuModel mModel{ *mGpu, Model(), "model" };
    GpuTexture mImage{ *mGpu, Image(), "image" };
    GpuTexture mTarget{ *mGpu,
                        GpuTextureOptions{ .mSize = ivec2( cTargetSize ),
                                           .mMips = 1,
                                           .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                           .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                           .mLabel = "target" } };
    GpuBuffer mTint{ *mGpu, WGPUBufferUsage_Uniform, 16, "tint" };

    Drawing() { mTint.Write( *mGpu, 0, AsBytes( vec4( 1.0f ) ) ); }

    // The quad's level `lod`, through the fragment stage `fragmentEntry`,
    // over transparent black: the target's pixels.
    vector<u8> Draw( const string& fragmentEntry, size_t lod )
    {
        Gpu& gpu = *mGpu;
        Required( gpu.TryCheckNow( [&] {
            const GpuShaderHandle shader = CreateShader( gpu, cShader, "textured" );
            const GpuRenderPipelineHandle pipeline =
                CreateRenderPipeline( gpu, GpuPipelineOptions{ .mLabel = "textured",
                                                               .mShader = shader.Get(),
                                                               .mVertexEntry = "vs_main",
                                                               .mFragmentEntry = fragmentEntry,
                                                               .mBindGroupLayouts = {},
                                                               .mVertexBuffers = { ModelVertexLayout() },
                                                               .mColorFormat = mTarget.Format(),
                                                               .mTopology = WGPUPrimitiveTopology_TriangleList,
                                                               .mCullMode = WGPUCullMode_Back } );

            WGPUSamplerDescriptor samplerDescriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
            const GpuSamplerHandle sampler( wgpuDeviceCreateSampler( gpu.Device(), &samplerDescriptor ) );
            array<WGPUBindGroupEntry, 3> entries{ WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
                                                  WGPU_BIND_GROUP_ENTRY_INIT };
            entries[0].binding = 0;
            entries[0].textureView = mImage.View();
            entries[1].binding = 1;
            entries[1].sampler = sampler.Get();
            entries[2].binding = 2;
            entries[2].buffer = mTint.Get();
            entries[2].size = mTint.Size();
            const GpuBindGroupLayoutHandle layout( wgpuRenderPipelineGetBindGroupLayout( pipeline.Get(), 0 ) );
            WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
            bindGroupDescriptor.layout = layout.Get();
            bindGroupDescriptor.entryCount = entries.size();
            bindGroupDescriptor.entries = entries.data();
            const GpuBindGroupHandle bindGroup( wgpuDeviceCreateBindGroup( gpu.Device(), &bindGroupDescriptor ) );

            const GpuCommandEncoderHandle encoder( wgpuDeviceCreateCommandEncoder( gpu.Device(), nullptr ) );
            WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
            attachment.view = mTarget.View();
            attachment.loadOp = WGPULoadOp_Clear;
            attachment.storeOp = WGPUStoreOp_Store;
            attachment.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 0.0 };
            WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
            passDescriptor.colorAttachmentCount = 1;
            passDescriptor.colorAttachments = &attachment;
            const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &passDescriptor ) );
            wgpuRenderPassEncoderSetPipeline( pass.Get(), pipeline.Get() );
            wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, bindGroup.Get(), 0, nullptr );
            mModel.Bind( pass.Get() );
            mModel.Draw( pass.Get(), 1, lod );
            wgpuRenderPassEncoderEnd( pass.Get() );
            gpu.Submit( encoder );
        } ) );
        return Required( gpu.TryReadRgba8( mTarget ) );
    }
};

// The image's texel at the target's pixel: each quarter of the target is
// one texel.
string TexelUnder( i32 x, i32 y )
{
    const array<string, 4> texels{ "255 0 0 255", "0 255 0 255", "0 0 255 255", "255 255 255 255" };
    return texels[static_cast<size_t>( ( y / 2 ) * 2 + x / 2 )];
}
}

TEST_CASE( "A model's meshes share one vertex and one index buffer, each level of detail a range of it" )
{
    const Scope<Gpu> gpu = StartGpu();
    const GpuModel model( *gpu, Model(), "model" );

    REQUIRE( model.Meshes().size() == 2 );
    const GpuMesh& first = model.Meshes()[0];
    CHECK( first.mMaterial == "first" );
    CHECK( first.mBaseVertex == 0 );
    REQUIRE( first.mLods.size() == 1 );
    CHECK( first.mLods[0].mFirstIndex == 0 );
    CHECK( first.mLods[0].mIndexCount == 3 );

    const GpuMesh& quad = model.Meshes()[1];
    CHECK( quad.mMaterial == "screen" );
    CHECK( quad.mBaseVertex == 3 );
    REQUIRE( quad.mLods.size() == 2 );
    CHECK( quad.mLods[0].mFirstIndex == 3 );
    CHECK( quad.mLods[0].mIndexCount == 6 );
    CHECK( quad.mLods[1].mFirstIndex == 9 );
    CHECK( quad.mLods[1].mIndexCount == 3 );
    CHECK( quad.mLods[1].mError == 0.5f );
    CHECK( model.Max() == vec3( 1.0f, 1.0f, 0.0f ) );
}

TEST_CASE( "A textured model drawn into a texture shows the image's colors" )
{
    Drawing drawing;
    const vector<u8> pixels = drawing.Draw( "fs_main", 0 );
    for ( i32 y = 0; y < cTargetSize; ++y )
        for ( i32 x = 0; x < cTargetSize; ++x )
        {
            CAPTURE( x );
            CAPTURE( y );
            CHECK( PixelAt( pixels, cTargetSize, x, y ) == TexelUnder( x, y ) );
        }
}

TEST_CASE( "A coarser level of detail draws its own range of the indices" )
{
    Drawing drawing;
    const vector<u8> pixels = drawing.Draw( "fs_main", 1 );
    // The diagonal runs through the pixels where x + y is 3; it is the
    // GPU's to say which of those its edge takes.
    for ( i32 y = 0; y < cTargetSize; ++y )
        for ( i32 x = 0; x < cTargetSize; ++x )
        {
            CAPTURE( x );
            CAPTURE( y );
            if ( x + y > 3 )
                CHECK( PixelAt( pixels, cTargetSize, x, y ) == TexelUnder( x, y ) );
            else if ( x + y < 3 )
                CHECK( PixelAt( pixels, cTargetSize, x, y ) == "0 0 0 0" );
        }
}

TEST_CASE( "Every level of an imported image reaches the GPU" )
{
    Drawing drawing;
    CHECK( drawing.mImage.Mips() == 2 );
    CHECK( drawing.mImage.MipSize( 1 ) == ivec2( 1 ) );
    const vector<u8> pixels = drawing.Draw( "fs_second_level", 0 );
    for ( i32 y = 0; y < cTargetSize; ++y )
        for ( i32 x = 0; x < cTargetSize; ++x )
        {
            CAPTURE( x );
            CAPTURE( y );
            CHECK( PixelAt( pixels, cTargetSize, x, y ) == "64 128 192 255" );
        }
}

TEST_CASE( "A WGSL mistake comes back from the GPU's check with its line" )
{
    const Scope<Gpu> gpu = StartGpu();
    constexpr string_view cBroken = R"wgsl(@vertex
fn vs_main() -> @builtin( position ) vec4f
{
    return nothing;
}
)wgsl";
    const auto checked =
        gpu->TryCheckNow( [&] { const GpuShaderHandle shader = CreateShader( *gpu, cBroken, "broken" ); } );
    REQUIRE_FALSE( checked );
    // wgpu-native's words: the shader by its label, what is wrong, where.
    CHECK( checked.error().find( "Shader 'broken' parsing error" ) != string::npos );
    CHECK( checked.error().find( "no definition in scope for identifier: `nothing`" ) != string::npos );
    CHECK( checked.error().find( "wgsl:4:12" ) != string::npos );

    // A check catches only what was made inside it.
    Required( gpu->TryCheckNow( [&] { const GpuShaderHandle shader = CreateShader( *gpu, cShader, "textured" ); } ) );
}

TEST_CASE( "A buffer is written in fours, or up to its end" )
{
    const Scope<Gpu> gpu = StartGpu();
    const GpuBuffer buffer( *gpu, WGPUBufferUsage_Uniform, 15, "fifteen" );
    const array<u8, 3> three{ 1, 2, 3 };
    CHECK( buffer.Size() == 15 );
    CHECK_NOTHROW( buffer.Write( *gpu, 12, AsBytes( three ) ) );
    CHECK_THROWS_AS( buffer.Write( *gpu, 0, AsBytes( three ) ), logic_error );
    CHECK_THROWS_AS( buffer.Write( *gpu, 2, AsBytes( three ) ), logic_error );
    CHECK_THROWS_AS( buffer.Write( *gpu, 16, AsBytes( three ) ), logic_error );
}
#endif
