#include "engine/engine.hpp"

namespace bubble
{
void Engine::OnUpdate()
{
    mTimer.OnUpdate();
    auto dt = mTimer.GetDeltaTime();
}

void Engine::DrawScene( const Camera& camera, 
                        const Framebuffer& framebuffer )
{
    auto framebufferSize = framebuffer.Size();
    auto projectionMat = camera.GetPprojectionMat( framebufferSize.x, framebufferSize.y );
    auto lookAtMat = camera.GetLookatMat();

    // Uniform buffer
    auto vertexBufferElement = mRenderer.mVertexUniformBuffer->Element( 0 );
    vertexBufferElement.SetMat4( "uProjection", projectionMat );
    vertexBufferElement.SetMat4( "uView", lookAtMat );

    auto fragmentBufferElement = mRenderer.mLightsInfoUniformBuffer->Element( 0 );
    fragmentBufferElement.SetInt( "uNumLights", 0 );
    fragmentBufferElement.SetFloat3( "uViewPos", camera.mPosition );

    // Scene rendering
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mRunningScene.ForEach<ModelComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.Transform(), model->mShader );
    } );
}


}
