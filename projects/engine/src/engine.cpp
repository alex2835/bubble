#include "engine/engine.hpp"

namespace bubble
{

Engine::Engine( WindowInput& input, Loader& loader )
    : mScriptingEngine( input, loader, mScene )
{

}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();

    // Update scripts
    mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& script )
    {
        mScriptingEngine.OnUpdate( script.mScript );
    });
}

void Engine::DrawScene( Framebuffer& framebuffer )
{
    // Scene rendering
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mRenderer.SetUniformBuffers( mActiveCamera, framebuffer );
    
    mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity _,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.TransformMat(), shader );
    } );
}

}
