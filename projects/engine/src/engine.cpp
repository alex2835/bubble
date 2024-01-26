#include "engine/engine.hpp"

namespace bubble
{
void Engine::OnUpdate()
{
    mTimer.OnUpdate();
    auto dt = mTimer.GetDeltaTime();
}

void Engine::DrawScene( const Camera& camera, const Framebuffer& framebuffer )
{
    // Uniform buffer
    auto vertexBufferElement = mRenderer.mVertexUniformBuffer->GetElement( 0 );
    auto size = framebuffer.GetSize();

    vertexBufferElement.SetMat4( "uProjection", camera.GetPprojectionMat( size.x, size.y ) );
    vertexBufferElement.SetMat4( "uView", camera.GetLookatMat() );
    

    framebuffer.Bind();
    mRenderer.ClearScreen(vec4(0.2f, 0.3f, 0.3f, 1.0f));
    mScene.ForEach<ModelComponent, TransformComponent>( [&]( Entity,
                                                             ModelComponent& model,
                                                             TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform );
    } );
}

}
