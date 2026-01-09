#include "engine/pch/pch.hpp"
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include <sol/sol.hpp>

namespace bubble
{
Engine::Engine( Project& project )
    : mProject( project ),
      mWhiteShader( LoadShader( WHITE_SHADER ) ),
      mBoundingBoxes{ .mMesh = Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },
      mPhysicsObjects{ .mMesh=Mesh( "Physics", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) }
{}

void Engine::OnStart()
{
    /// Physics
    mProject.mPhysicsEngine.ClearWorld();

    // Set physics init transform
    mProject.mScene.ForEach<TransformComponent, PhysicsComponent>(
    [&]( Entity entity, TransformComponent& transform, PhysicsComponent& physics )
    {
        physics.mPhysicsObject.SetTransform( transform.mPosition, transform.mRotation );
        physics.mPhysicsObject.ClearForces();
        mProject.mPhysicsEngine.Add( physics.mPhysicsObject, entity );
    } );

    /// Scripts
    // Extract scripts functions
    mProject.mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& scriptComponent )
    {
        if ( not scriptComponent.mScript )
            throw std::runtime_error( std::format( "Entity:{} Script not set", (u64)entity ) );
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
    []( Entity entity,
        TransformComponent& transform,
        const PhysicsComponent& physics )
    {
         physics.mPhysicsObject.GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Call scripts
    mProject.mScene.ForEach<StateComponent, ScriptComponent>( 
    []( Entity entity,
        const StateComponent& stateComponent,
        const ScriptComponent& scriptComponent )
    {
        if ( scriptComponent.mOnUpdate )
        {
            sol::protected_function_result result = scriptComponent.mOnUpdate( entity, *stateComponent.mState );
            if ( !result.valid() )
            {
                sol::error err = result;
                throw std::runtime_error( err.what() );
            }
        }
    });
}

void Engine::DrawScene( Framebuffer& framebuffer )
{
    DrawScene( framebuffer, mProject.mScene );
}

void Engine::DrawScene( Framebuffer& framebuffer,
                        Scene& scene )
{
    // Set up lights
    std::vector<Light> lights;
    scene.ForEach<TransformComponent, LightComponent>(
        [&]( Entity _,
                  const TransformComponent& transformComponent,
                  LightComponent& lightComponent )
    {
        lightComponent.mDirection = transformComponent.Forward();
        lightComponent.mPosition = transformComponent.mPosition;
        lights.push_back( (Light)lightComponent );
    } );


    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );

    mRenderer.SetCameraUniformBuffers( mActiveCamera, framebuffer );
    mRenderer.SetLightsUniformBuffer( mActiveCamera, lights );

    // Render models
    scene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
        [&]( Entity _,
             ModelComponent& modelComponent,
             ShaderComponent& shaderComponent,
             TransformComponent& transform )
    {
        mRenderer.DrawModel( modelComponent.mModel, shaderComponent.mShader, transform.TransformMat() );
    } );
}


void Engine::DrawBoundingBoxes( Framebuffer& framebuffer, Scene& scene )
{
    framebuffer.Bind();
    if ( scene.Size() == 0 )
        return;

    u32 elementIndexStride = 0;
    mBoundingBoxes.mVertices.Clear();
    mBoundingBoxes.mIndices.clear();

    scene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity _,
                  const ModelComponent& model,
                  const TransformComponent& transform )
    {
        if ( not model.mModel )
            return;

        const mat4 trans = transform.TransformMat();
        const AABB box = CalculateTransformedBBox( model.mModel->mBBox, trans );
        const auto [vertices, indices] = CalculateBBoxShapeData( box );
        for ( vec3 vertex : vertices )
            mBoundingBoxes.mVertices.mPositions.push_back( vertex );
        for ( u32 index : indices  )
            mBoundingBoxes.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mBoundingBoxes.mVertices.mPositions.size();
    } );
    mBoundingBoxes.mMesh.UpdateDynamicVertexBufferData( mBoundingBoxes.mVertices, mBoundingBoxes.mIndices );
    mRenderer.DrawMesh( mBoundingBoxes.mMesh, mWhiteShader, glm::identity<mat4>(), DrawingPrimitive::Lines );
}


void Engine::DrawPhysicsShapes( Framebuffer& framebuffer, Scene& scene )
{
    framebuffer.Bind();
    if ( scene.Size() == 0 )
        return;

    u32 elementIndexStride = 0;
    mPhysicsObjects.mVertices.Clear();
    mPhysicsObjects.mIndices.clear();

    scene.ForEach<PhysicsComponent, TransformComponent>(
        [&]( Entity _,
             PhysicsComponent& physics,
             const TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();
        const auto& [vertices, indices] = physics.mPhysicsObject.mShapeData;
        for ( auto vertex : vertices )
            mPhysicsObjects.mVertices.mPositions.push_back( vec3( trans * vec4( vertex, 1 ) ) );
        for ( u32 index : indices )
            mPhysicsObjects.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mPhysicsObjects.mVertices.mPositions.size();
    } );
    mPhysicsObjects.mMesh.UpdateDynamicVertexBufferData( mPhysicsObjects.mVertices, mPhysicsObjects.mIndices );
    mRenderer.DrawMesh( mPhysicsObjects.mMesh, mWhiteShader, glm::identity<mat4>(), DrawingPrimitive::Lines );
}


}
