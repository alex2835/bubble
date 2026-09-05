#include "engine/pch/pch.hpp"
#include "engine/renderer/renderer.hpp"
#include "engine/renderer/gpu_context.hpp"
#include "engine/utils/geometry.hpp"
#include "engine/log/log.hpp"

namespace bubble
{

RenderTarget RenderTarget::For( wgpu::RenderPassEncoder pass, const Framebuffer& framebuffer )
{
    RenderTarget target;
    target.mPass = pass;
    target.mColorFormat = ToWGPU( framebuffer.ColorAttachment().Specification().mFormat );
    target.mDepthFormat = wgpu::TextureFormat::Depth32Float;
    return target;
}


Renderer::Renderer()
{
    // Blend, depth test and face culling used to be set here once and never
    // touched again. They are pipeline state now - see PipelineKey - so there
    // is nothing global left to configure.

    // One buffer per frame block. Sizes come from the mirror structs, which
    // static_assert against the WGSL - there is no runtime layout to compute.
    const auto uniformBuffer = []( const char* label, u64 size )
    {
        wgpu::BufferDescriptor desc = wgpu::Default;
        desc.label = wgpu::StringView( label );
        desc.size = size;
        desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        desc.mappedAtCreation = false;
        return wgpu::raii::Buffer( Gpu().Device().createBuffer( desc ) );
    };

    constexpr u64 cLightsBufferSize = sizeof( LightUniforms ) * cMaxLights;
    mVertexUniformBuffer = uniformBuffer( "VertexUniformBuffer", sizeof( VertexUniforms ) );
    mLightsInfoUniformBuffer = uniformBuffer( "LightsInfoUniformBuffer", sizeof( LightsInfoUniforms ) );
    mLightsUniformBuffer = uniformBuffer( "LightsUniformBuffer", cLightsBufferSize );
    mLightUniforms.reserve( cMaxLights );

    // The three blocks never move, so the frame bind group is built once.
    array<wgpu::BindGroupEntry, 3> entries = {};
    const wgpu::Buffer buffers[3] = { *mVertexUniformBuffer,
                                      *mLightsInfoUniformBuffer,
                                      *mLightsUniformBuffer };
    const u64 sizes[3] = { sizeof( VertexUniforms ),
                           sizeof( LightsInfoUniforms ),
                           cLightsBufferSize };
    for ( u32 i = 0; i < 3; i++ )
    {
        entries[i] = {};
        entries[i].binding = i;
        entries[i].buffer = buffers[i];
        entries[i].offset = 0;
        entries[i].size = sizes[i];
    }

    wgpu::BindGroupDescriptor desc = wgpu::Default;
    desc.label = wgpu::StringView( "Frame Bind Group" );
    desc.layout = Gpu().Layouts().Frame();
    desc.entryCount = entries.size();
    desc.entries = entries.data();
    mFrameBindGroup = wgpu::raii::BindGroup( Gpu().Device().createBindGroup( desc ) );

    mDrawRing.Init( sizeof( DrawUniforms ), Gpu().Layouts().Draw(), "Draw Uniform Ring" );
    mUserRing.Init( cUserUniformBlockSize, Gpu().Layouts().User(), "User Uniform Ring" );
}

void Renderer::SetUserUniforms( const void* data, u64 size )
{
    mPendingUserData = data;
    mPendingUserSize = size;
}

void Renderer::BeginFrame()
{
    mDrawRing.Reset();
    mUserRing.Reset();
}

void Renderer::FlushDrawUniforms()
{
    mDrawRing.Flush();
    mUserRing.Flush();
}

void Renderer::SetCameraUniformBuffers( const Camera& camera, const Framebuffer& framebuffer )
{
    auto framebufferSize = framebuffer.Size();
    auto projectionMat = camera.GetProjectionMat( framebufferSize.x, framebufferSize.y );
    auto lookAtMat = camera.GetLookatMat();

    mVertexUniforms.mProjection = projectionMat;
    mVertexUniforms.mView = lookAtMat;
}

void Renderer::SetLightsUniformBuffer( const Camera& camera, const vector<Light>& lights )
{
    mLightsInfoUniforms.mNumLights = (i32)lights.size();
    mLightsInfoUniforms.mViewPos = camera.mPosition;

    mLightUniforms.clear();
    for ( const auto& light : lights )
    {
        LightUniforms& uniforms = mLightUniforms.emplace_back();
        uniforms.mType = (i32)light.mType;
        uniforms.mBrightness = light.mBrightness;
        uniforms.mConstant = light.mConstant;
        uniforms.mLinear = light.mLinear;
        uniforms.mQuadratic = light.mQuadratic;
        // Degrees on the scene side, cosines on the shader side.
        uniforms.mCutOff = glm::cos( glm::radians( light.mCutOff ) );
        uniforms.mOuterCutOff = glm::cos( glm::radians( light.mOuterCutOff ) );
        uniforms.mColor = light.mColor;
        uniforms.mDirection = light.mDirection;
        uniforms.mPosition = light.mPosition;
    }
}

void Renderer::FlushFrameUniforms()
{
    wgpu::Queue queue = Gpu().Queue();
    queue.writeBuffer( *mVertexUniformBuffer, 0, &mVertexUniforms, sizeof( mVertexUniforms ) );
    queue.writeBuffer( *mLightsInfoUniformBuffer, 0, &mLightsInfoUniforms, sizeof( mLightsInfoUniforms ) );

    // Only the lights actually present, not all 128 slots. A scene with no
    // lights still writes one slot - uNumLights is zero, so the shader never
    // reads it, but a zero sized write is not worth special casing here.
    if ( mLightUniforms.empty() )
    {
        const LightUniforms empty;
        queue.writeBuffer( *mLightsUniformBuffer, 0, &empty, sizeof( empty ) );
    }
    else
    {
        queue.writeBuffer( *mLightsUniformBuffer, 0, mLightUniforms.data(),
                           sizeof( LightUniforms ) * mLightUniforms.size() );
    }
}

void Renderer::BindFrame( wgpu::RenderPassEncoder pass )
{
    pass.setBindGroup( (u32)BindGroupIndex::Frame, *mFrameBindGroup, 0, nullptr );
}

void Renderer::DrawMeshPrimitives( const RenderTarget& target,
                                   const Mesh& mesh,
                                   const Ref<Shader>& shader,
                                   DrawingPrimitive drawingPrimitive,
                                   u32 dynamicOffset )
{
    const VertexLayout& layout = mesh.mVertexArray.Layout();
    if ( not mesh.mVertexArray.Valid() )
        return;

    PipelineKey key;
    key.mColorFormat = target.mColorFormat;
    key.mDepthFormat = target.mDepthFormat;
    key.mPrimitive = drawingPrimitive;
    key.mCullBackFaces = drawingPrimitive == DrawingPrimitive::Triangles;
    key.mBlend = false;
    key.mAttributeMask = VertexLayoutAttributeMask( layout );

    wgpu::RenderPipeline pipeline = shader->GetPipeline( key, layout );
    if ( not pipeline )
        return;

    target.mPass.setPipeline( pipeline );

    // The draw group is always bound, even for shaders that read nothing from
    // it - a pipeline layout entry has to be satisfied.
    target.mPass.setBindGroup( (u32)BindGroupIndex::Draw, mDrawRing.GetBindGroup(),
                               1, &dynamicOffset );

    // Likewise the shader's own uniforms. A shader that declares no
    // UserUniforms block still gets a slot bound; it just never reads it.
    u32 userOffset = mUserRing.Push( mPendingUserData, mPendingUserSize );
    target.mPass.setBindGroup( (u32)BindGroupIndex::User, mUserRing.GetBindGroup(),
                               1, &userOffset );

    // Always bound, not just when the shader includes <material>. The pipeline
    // layout declares all three groups, and a group the layout names has to
    // have something set before a draw even if the shader never samples it.
    // Meshes carry a default material, so there is always something to bind.
    mesh.ApplyMaterial( target.mPass );

    mesh.BindVertexArray( target.mPass );
    target.mPass.drawIndexed( (u32)mesh.IndiciesSize(), 1, 0, 0, 0 );
}

void Renderer::DrawMesh( const RenderTarget& target,
                         const Mesh& mesh,
                         const Ref<Shader>& shader,
                         const mat4& transform,
                         DrawingPrimitive drawingPrimitive,
                         u32 objectId,
                         const DrawUniforms* extras )
{
    if ( not shader or not shader->Valid() )
    {
        BUBBLE_ASSERT( false, "DrawMesh: shader is null" );
        return;
    }

    // Billboards pass their position, size and tint through here rather than
    // through loose uniforms, which no longer exist.
    DrawUniforms uniforms = extras ? *extras : DrawUniforms{};
    uniforms.mModel = transform;
    uniforms.SetNormalMatrix( CalculateNormalMat( transform ) );
    uniforms.mObjectId = objectId;
    const u32 dynamicOffset = mDrawRing.Push( &uniforms, sizeof( uniforms ) );

    DrawMeshPrimitives( target, mesh, shader, drawingPrimitive, dynamicOffset );
    SetUserUniforms( nullptr, 0 );
}

void Renderer::DrawModel( const RenderTarget& target,
                          const Ref<Model>& model,
                          const Ref<Shader>& shader,
                          const mat4& transform,
                          DrawingPrimitive drawingPrimitive,
                          u32 objectId )
{
    if ( not model or not shader or not shader->Valid() )
    {
        BUBBLE_ASSERT( false, "DrawModel: Model or shader is null" );
        return;
    }

    // One slot for the whole model: every mesh in it shares the transform.
    DrawUniforms uniforms;
    uniforms.mModel = transform;
    uniforms.SetNormalMatrix( CalculateNormalMat( transform ) );
    uniforms.mObjectId = objectId;
    const u32 dynamicOffset = mDrawRing.Push( &uniforms, sizeof( uniforms ) );

    for ( const auto& mesh : model->mMeshes )
        DrawMeshPrimitives( target, mesh, shader, drawingPrimitive, dynamicOffset );
    SetUserUniforms( nullptr, 0 );
}

}
