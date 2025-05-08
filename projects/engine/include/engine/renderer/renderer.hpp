#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"
#include "engine/scene/scene.hpp"

namespace bubble
{
enum class DrawingPrimitive
{
    Triangles = 0x0004,
    Lines = 0x0001,
    Points = 0x0000
};



class Renderer
{
public:
    Renderer();
    void ClearScreen( vec4 color );
    void ClearScreenUint( uvec4 color );
    void SetUniformBuffers( const Camera& camera, const Framebuffer& framebuffer );

    void DrawMesh( const Mesh& mesh, 
                   const Ref<Shader>& shader, 
                   DrawingPrimitive drawingPrimitive );

    void DrawMesh( const Mesh& mesh, 
                   const Ref<Shader>& shader, 
                   const mat4& transform, 
                   DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles );

    void DrawModel( const Ref<Model>& model, 
                    const mat4& transform, 
                    const Ref<Shader>& shader, 
                    DrawingPrimitive drawingPrimitive = DrawingPrimitive::Triangles );

//private:
    Ref<UniformBuffer> mVertexUniformBuffer;
    Ref<UniformBuffer> mLightsInfoUniformBuffer;
    Ref<UniformBuffer> mLightsUniformBuffer;
};

}