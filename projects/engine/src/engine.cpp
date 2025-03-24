
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"

namespace bubble
{
Engine::Engine( Project& project )
    : mProject( project ),
      mScene( project.mGameRunningScene )
{}

void Engine::OnStart()
{
    // Game running scene that is binded to scripting engine
    mScene = mProject.mScene;

    // Extract scripts functions
    mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& scriptComponent )
    {
        if ( not scriptComponent.mOnUpdate )
            scriptComponent.mOnUpdate = mProject.mScriptingEngine.ExtractOnUpdate( scriptComponent.mScript );
    } );
}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();

    // Call scripts
    mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& script )
    {
        if ( script.mOnUpdate )
            script.mOnUpdate();
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
