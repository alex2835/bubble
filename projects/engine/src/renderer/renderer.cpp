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

    VertexBufferLayout vertexUniformVertexBufferLayout{
        { "uProjection", GLSLDataType::Mat4  },
        { "uView", GLSLDataType::Mat4  }
    };
    mVertexUniformBuffer = CreateRef<UniformBuffer>( 0,
                                                     "VertexUniformBuffer",
                                                     std::move( vertexUniformVertexBufferLayout ) );

    VertexBufferLayout lightsInfoUniformVertexBufferLayout{
        { "uNumLights", GLSLDataType::Int },
        { "uViewPos", GLSLDataType::Float3 }
    };
    mLightsInfoUniformBuffer = CreateRef<UniformBuffer>( 1,
                                                         "LightsInfoUniformBuffer",
                                                         std::move( lightsInfoUniformVertexBufferLayout ) );

    VertexBufferLayout lightsUniformVertexBufferLayout{
        { "type", GLSLDataType::Int },
        { "brightness", GLSLDataType::Float },
        { "constant", GLSLDataType::Float },
        { "linear", GLSLDataType::Float },
        { "quadratic", GLSLDataType::Float },
        { "cutOff", GLSLDataType::Float },
        { "outerCutOff", GLSLDataType::Float },
        { "color", GLSLDataType::Float3 },
        { "direction", GLSLDataType::Float3 },
        { "position", GLSLDataType::Float3 },
    };
    mLightsUniformBuffer = CreateRef<UniformBuffer>( 2,
                                                     "LightsUniformBuffer",
                                                     std::move( lightsUniformVertexBufferLayout ),
                                                     cMaxLights );

    // The three blocks never move, so the frame bind group is built once.
    array<wgpu::BindGroupEntry, 3> entries = {};
    const UniformBuffer* buffers[3] = { mVertexUniformBuffer.get(),
                                        mLightsInfoUniformBuffer.get(),
                                        mLightsUniformBuffer.get() };
    for ( u32 i = 0; i < 3; i++ )
    {
        entries[i] = {};
        entries[i].binding = i;
        entries[i].buffer = buffers[i]->GetBuffer();
        entries[i].offset = 0;
        entries[i].size = buffers[i]->BufferSize();
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

    auto vertexBufferElement = mVertexUniformBuffer->Element( 0 );
    vertexBufferElement.SetMat4( "uProjection", projectionMat );
    vertexBufferElement.SetMat4( "uView", lookAtMat );
}

void Renderer::SetLightsUniformBuffer( const Camera& camera, const vector<Light>& lights )
{
    auto fragmentBufferElement = mLightsInfoUniformBuffer->Element( 0 );
    fragmentBufferElement.SetInt( "uNumLights", (i32)lights.size() );
    fragmentBufferElement.SetFloat3( "uViewPos", camera.mPosition );

    for ( i32 i = 0; i < (i32)lights.size(); i++ )
    {
        const auto& light = lights[i];
        auto lightBufferElement = mLightsUniformBuffer->Element( i );
        lightBufferElement.SetInt( "type", (i32)light.mType );
        lightBufferElement.SetFloat( "brightness", light.mBrightness );
        lightBufferElement.SetFloat( "constant", light.mConstant );
        lightBufferElement.SetFloat( "linear", light.mLinear );
        lightBufferElement.SetFloat( "quadratic", light.mQuadratic );
        lightBufferElement.SetFloat( "cutOff", glm::cos( glm::radians( light.mCutOff ) ) );
        lightBufferElement.SetFloat( "outerCutOff", glm::cos( glm::radians( light.mOuterCutOff ) ) );
        lightBufferElement.SetFloat3( "color", light.mColor );
        lightBufferElement.SetFloat3( "direction", light.mDirection );
        lightBufferElement.SetFloat3( "position", light.mPosition );
    }
    mLightCount = lights.size();
}

void Renderer::FlushFrameUniforms()
{
    mVertexUniformBuffer->Flush();
    mLightsInfoUniformBuffer->Flush();
    // Only the lights actually present, not all 128 slots.
    mLightsUniformBuffer->Flush( std::max<u64>( mLightCount, 1 ) );
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
