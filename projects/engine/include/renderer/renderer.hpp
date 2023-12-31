#pragma once
#include "renderer/camera.hpp"
#include "renderer/buffer.hpp"
#include "renderer/framebuffer.hpp"
#include "renderer/model.hpp"

namespace bubble
{

class BUBBLE_ENGINE_EXPORT Renderer
{
public:
    void DrawMesh( const Mesh& mesh );
    void DrawModel( const Ref<Model>& model );

private:
    Camera mCamera;
    Framebuffer mFramebuffer;
};

}