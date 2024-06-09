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
    // Scene rendering
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mRenderer.SetUniformBuffers( camera, framebuffer );
    mRunningScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.Transform(), shader );
    } );
}


}
