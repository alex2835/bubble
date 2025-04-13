
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"

#include <print>

namespace bubble
{
Engine::Engine( Project& project )
    : mProject( project )
{}

void Engine::OnStart()
{
    /// Physics
    mProject.mPhysicsEngine.ClearWorld();

    // Set physics init transform
    mProject.mScene.ForEach<TransformComponent, PhysicsComponent>(
    [&]( Entity entity, TransformComponent& transform, PhysicsComponent& physics )
    {
        std::println( "entity:{}", u64(entity) );

        physics.mPhysicsObject->SetTransform( transform.mPosition, transform.mRotation );
        physics.mPhysicsObject->ClearForces();
        mProject.mPhysicsEngine.AddPhysicsObject( physics.mPhysicsObject );
    } );

    /// Scripts
    // Extract scripts functions
    mProject.mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& scriptComponent )
    {
        if ( not scriptComponent.mScript )
            std::runtime_error( std::format( "Entity:{} Script not set", (u64)entity ) );
        mProject.mScriptingEngine.ExtractOnUpdate( scriptComponent.mOnUpdate, scriptComponent.mScript );
    } );
}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();
    auto dt = mTimer.GetDeltaTime();

    mProject.mPhysicsEngine.Update( dt );

    // Update transforms
    mProject.mScene.ForEach<TransformComponent, PhysicsComponent>(
    []( Entity entity, TransformComponent& transform, PhysicsComponent& physics )
    {
         physics.mPhysicsObject->GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Call scripts
    mProject.mScene.ForEach<ScriptComponent>( []( Entity entity, ScriptComponent& script )
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
    
    mProject.mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
    [&]( Entity _,
         ModelComponent& model,
         ShaderComponent& shader,
         TransformComponent& transform )
    {
        mRenderer.DrawModel( model, transform.TransformMat(), shader );
    } );
}

}
