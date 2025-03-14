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
    auto view = mScene.GetView<ScriptComponent>();
    for ( auto entity : view.GetEntities() )
    {
        auto& scirptingComponent = entity.GetComponent<ScriptComponent>();
        mScriptingEngine.OnUpdate( scirptingComponent.mScript );
    }
}

void Engine::DrawScene( const Framebuffer& framebuffer )
{
    // Scene rendering
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mRenderer.SetUniformBuffers( mActiveCamera, framebuffer );
    
    mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity entity,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.Transform(), shader );
    } );
}

}
