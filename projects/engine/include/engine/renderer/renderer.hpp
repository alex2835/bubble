#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/buffer.hpp"
#include "engine/renderer/framebuffer.hpp"
#include "engine/renderer/model.hpp"

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