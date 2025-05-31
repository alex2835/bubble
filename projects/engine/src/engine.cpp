
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"

#include <print>

namespace bubble
{
Engine::Engine( Project& project )
    : mProject( project ),
      mWhiteShader( LoadShader( WHITE_SHADER ) ),
      mBoudingBoxes{ .mMesh = Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },
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
        mProject.mPhysicsEngine.Add( physics.mPhysicsObject );
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
    []( Entity entity, TransformComponent& transform, PhysicsComponent& physics )
    {
         physics.mPhysicsObject.GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Call scripts
    mProject.mScene.ForEach<ScriptComponent>( []( Entity entity, ScriptComponent& script )
    {
        if ( script.mOnUpdate )
        {
            sol::protected_function_result result = script.mOnUpdate();
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
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mRenderer.SetUniformBuffers( mActiveCamera, framebuffer );

    scene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
        [&]( Entity _,
             ModelComponent& modelComponent,
             ShaderComponent& shaderComponent,
             TransformComponent& transform )
    {
        mRenderer.DrawModel( modelComponent.mModel, transform.TransformMat(), shaderComponent.mShader );
    } );
}


void Engine::DrawBoundingBoxes( Framebuffer& framebuffer, Scene& scene )
{
    framebuffer.Bind();
    if ( scene.Size() == 0 )
        return;

    u32 elementIndexStride = 0;
    mBoudingBoxes.mVertices.Clear();
    mBoudingBoxes.mIndices.clear();

    scene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity _,
             ModelComponent& model,
             TransformComponent& transform )
    {
        const mat4 trans = transform.TransformMat();
        const AABB box = CalculateTransformedBBox( model.mModel->mBBox, trans );
        const auto [vertices, indices] = CalculateBBoxShapeData( box );
        for ( vec3 vertex : vertices )
            mBoudingBoxes.mVertices.mPositions.push_back( vertex );
        for ( u32 index : indices  )
            mBoudingBoxes.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mBoudingBoxes.mVertices.mPositions.size();
    } );
    mBoudingBoxes.mMesh.UpdateDynamicVertexBufferData( mBoudingBoxes.mVertices, mBoudingBoxes.mIndices );
    mRenderer.DrawMesh( mBoudingBoxes.mMesh, mWhiteShader, mat4( 1 ), DrawingPrimitive::Lines );
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
             TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();
        const auto& [vertices, indices] = physics.mPhysicsObject.mShapeData;
        for ( auto vertex : vertices )
            mPhysicsObjects.mVertices.mPositions.push_back( vec3( trans * vec4( vertex, 1 ) ) );
        for ( auto index : indices )
            mPhysicsObjects.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mPhysicsObjects.mVertices.mPositions.size();
    } );
    mPhysicsObjects.mMesh.UpdateDynamicVertexBufferData( mPhysicsObjects.mVertices, mPhysicsObjects.mIndices );
    mRenderer.DrawMesh( mPhysicsObjects.mMesh, mWhiteShader, mat4( 1 ), DrawingPrimitive::Lines );
}


}
