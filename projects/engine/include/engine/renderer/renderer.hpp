#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/light.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/pipeline.hpp"
#include <span>

namespace bubble
{

// DrawingPrimitive now lives in pipeline.hpp: topology is baked into a pipeline
// object rather than passed to the draw call.


// Where a draw is going. The formats are part of the pipeline key, so they have
// to travel with the pass.
struct RenderTarget
{
    wgpu::RenderPassEncoder mPass;
    wgpu::TextureFormat mColorFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat mDepthFormat = wgpu::TextureFormat::Depth32Float;

    static RenderTarget For( wgpu::RenderPassEncoder pass, const Framebuffer& framebuffer );
};


class Renderer
{
public:
    // Must match MAX_LIGHTS in resources/shaders/modules/light.wgsl. Nothing
    // checks the two agree: raise this without raising that and the extra
    // lights are silently clamped away by WGSL's bounds handling.
    //
    // 512 * sizeof( LightUniforms ) is 40 KiB, inside the 64 KiB that WebGPU
    // guarantees for maxUniformBufferBindingSize. Going much past ~819 needs
    // the array moved to a storage buffer.
    static constexpr int cMaxLights = 512;

    Renderer();

    // Resets the draw uniform ring. Once at the top of a frame.
    void BeginFrame();
    // Uploads everything staged so far this frame. Every draw entry point
    // submits its own command buffer, so this runs before each submit; offsets
    // stay valid because the ring only resets in BeginFrame.
    void FlushDrawUniforms();

    void SetCameraUniformBuffers( const Camera& camera, const Framebuffer& framebuffer );
    void SetLightsUniformBuffer( const Camera& camera, const vector<Light>& lights );

    // Pushes the camera and light blocks to the GPU. The old renderer wrote
    // every scalar straight through as its own sub-buffer upload; these are
    // staged on the CPU and sent in one go per block.
    void FlushFrameUniforms();

    // Sets the frame bind group. Once per render pass, not per draw.
    void BindFrame( wgpu::RenderPassEncoder pass );

    // Staged for the next draw only, then cleared. Keeps the draw entry points
    // from growing another parameter that almost every caller passes empty.
    void SetUserUniforms( const void* data, u64 size );

    void DrawMesh( const RenderTarget& target,
                   const Mesh& mesh,
                   const Ref<Shader>& shader,
                   const mat4& transform,
                   DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles,
                   u32 objectId = 0,
                   const DrawUniforms* extras = nullptr );

    // `skin` is the joint matrices of a posed skinned model - model space
    // pose times inverse bind, in skeleton order - bound for the vertex
    // stage to read. Empty draws the model at rest.
    void DrawModel( const RenderTarget& target,
                    const Ref<Model>& model,
                    const Ref<Shader>& shader,
                    const mat4& transform,
                    DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles,
                    u32 objectId = 0,
                    std::span<const mat4> skin = {} );

private:
    void DrawMeshPrimitives( const RenderTarget& target,
                             const Mesh& mesh,
                             const Ref<Shader>& shader,
                             DrawingPrimitive drawingPrimitive,
                             u32 drawOffset,
                             u32 skinOffset );
    // The draw group binds two rings; it is rebuilt when either grows.
    void RebuildDrawBindGroup();

    // The three frame blocks. Plain buffers with a POD struct each, the same
    // shape as the material and draw blocks - the layout is checked against
    // the WGSL by static_assert rather than described by name at runtime.
    wgpu::raii::Buffer mVertexUniformBuffer;
    wgpu::raii::Buffer mLightsInfoUniformBuffer;
    wgpu::raii::Buffer mLightsUniformBuffer;

    // Staged on the CPU, uploaded once per block by FlushFrameUniforms.
    VertexUniforms mVertexUniforms;
    LightsInfoUniforms mLightsInfoUniforms;
    vector<LightUniforms> mLightUniforms;

    wgpu::raii::BindGroup mFrameBindGroup;
    DynamicUniformRing mDrawRing;
    // One slot of cMaxSkinJoints matrices per skinned draw.
    DynamicUniformRing mSkinRing;
    wgpu::raii::BindGroup mDrawBindGroup;
    u64 mDrawBindGroupGeneration = 0;
    DynamicUniformRing mUserRing;
    // Set by SetUserUniforms, consumed by the next draw.
    const void* mPendingUserData = nullptr;
    u64 mPendingUserSize = 0;
};

}
