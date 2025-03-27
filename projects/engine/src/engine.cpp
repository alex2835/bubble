
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"

namespace bubble
{
Engine::Engine( Project& project )
    : mProject( project )
{}

void Engine::OnStart()
{
    // Game running scene that is binded to scripting engine
    mProject.mGameRunningScene = mProject.mScene;

    // Extract scripts functions
    mProject.mGameRunningScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& scriptComponent )
    {
        mProject.mScriptingEngine.ExtractOnUpdate( scriptComponent.mOnUpdate, scriptComponent.mScript );
    } );
}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();

    // Call scripts
    mProject.mGameRunningScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& script )
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
    
    mProject.mGameRunningScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity _,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.TransformMat(), shader );
    } );
}

}
