#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/light.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/renderer/shader.hpp"
#include "engine/renderer/pipeline.hpp"

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
    static constexpr int cMaxLights = 128;

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

    void DrawMesh( const RenderTarget& target,
                   const Mesh& mesh,
                   const Ref<Shader>& shader,
                   const mat4& transform,
                   DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles,
                   u32 objectId = 0,
                   const DrawUniforms* extras = nullptr );

    void DrawModel( const RenderTarget& target,
                    const Ref<Model>& model,
                    const Ref<Shader>& shader,
                    const mat4& transform,
                    DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles,
                    u32 objectId = 0 );

private:
    void DrawMeshPrimitives( const RenderTarget& target,
                             const Mesh& mesh,
                             const Ref<Shader>& shader,
                             DrawingPrimitive drawingPrimitive,
                             u32 dynamicOffset );

    Ref<UniformBuffer> mVertexUniformBuffer;
    Ref<UniformBuffer> mLightsInfoUniformBuffer;
    Ref<UniformBuffer> mLightsUniformBuffer;
    wgpu::raii::BindGroup mFrameBindGroup;
    DrawUniformRing mDrawRing;
    u64 mLightCount = 0;
};

}
