#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{

class BUBBLE_ENGINE_EXPORT Renderer
{
public:
    void ClearScreen( vec4 color );
    void DrawMesh( const Mesh& mesh, const Ref<Shader>& shader, const mat4& transform );
    void DrawModel( const Ref<Model>& model, const mat4& transform );

    Renderer();

//private:
    Ref<UniformBuffer> mVertexUniformBuffer;    
    Ref<UniformBuffer> mFragmentUniformBuffer;
};

}