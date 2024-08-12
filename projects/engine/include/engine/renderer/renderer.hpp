#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{

class Renderer
{
public:
    Renderer();
    void ClearScreen( vec4 color );
    void ClearScreenUint( uvec4 color );
    void SetUniformBuffers( const Camera& camera, const Framebuffer& framebuffer );
    void DrawMesh( const Mesh& mesh, const Ref<Shader>& shader, const mat4& transform );
    void DrawModel( const Ref<Model>& model, const mat4& transform, const Ref<Shader>& shader = {} );

//private:
    Ref<UniformBuffer> mVertexUniformBuffer;    
    Ref<UniformBuffer> mLightsInfoUniformBuffer;
    Ref<UniformBuffer> mLightsUniformBuffer;
};

}