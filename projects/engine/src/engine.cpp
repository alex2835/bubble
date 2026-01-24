#include "engine/pch/pch.hpp"
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/renderer/helpers/create_billboard.hpp"
#include <sol/sol.hpp>

namespace bubble
{
Engine::Engine( Window& window )
    : mWindow( window ),
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),
      mEntityIdBillboardShader( LoadShader( ENTITY_PICKING_BILLBOARD_SHADER ) ),

      mWhiteShader( LoadShader( WHITE_SHADER ) ),
      mBoundingBoxes{ .mMesh = Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },
      mPhysicsObjects{ .mMesh=Mesh( "Physics", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },

      // billboards
      mBillboardShader( LoadShader( BILBOARD_SHADER ) ),
      mBillboardQuad( CreateBillboardQuadMesh() ),
      mSceneCameraTexture( LoadTexture2D( SCENE_CAMERA_TEXTURE ) ),
      mScenePointLightTexture( LoadTexture2D( SCENE_POINT_LIGHT_TEXTURE ) ),
      mSceneSpotLightTexture( LoadTexture2D( SCENE_SPOT_LIGHT_TEXTURE ) ),
      mSceneDirLightTexture( LoadTexture2D( SCENE_DIR_LIGHT_TEXTURE ) ),

      // Error values
      mErrorModel( LoadModel( ERROR_MODEL ) ),
      mErrorTexture( LoadTexture2D( ERROR_TEXTURE ) )
{
    // Bind members to scripting engine
    mScriptingEngine.BindInput( window.GetWindowInput() );
    mScriptingEngine.BindLoader( mLoader );
    mScriptingEngine.BindScene( mScene, mPhysicsEngine );
}

Engine::~Engine()
{
    mPhysicsEngine.ClearWorld();
}

void Engine::OnStart()
{
    /// Physics
    mPhysicsEngine.ClearWorld();

    // Set physics init transform
    mScene.ForEach<TransformComponent, PhysicsComponent>(
    [&]( Entity entity, TransformComponent& transform, PhysicsComponent& physics )
    {
        physics.mPhysicsObject.SetTransform( transform.mPosition, transform.mRotation );
        physics.mPhysicsObject.ClearForces();
        mPhysicsEngine.Add( physics.mPhysicsObject, entity );
    } );

    /// Scripts
    // Extract scripts functions
    mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& scriptComponent )
    {
        if ( not scriptComponent.mScript )
            throw std::runtime_error( std::format( "Entity:{} Script not set", (u64)entity ) );
        mScriptingEngine.ExtractOnUpdate( scriptComponent.mOnUpdate, scriptComponent.mScript );
    } );
}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();
    auto dt = mTimer.GetDeltaTime();

    // Update physics world
    mPhysicsEngine.Update( dt );

    // Propagate transforms
    PropagateTransforms( mScene );

    // Update transforms from physics world
    mScene.ForEach<TransformComponent, PhysicsComponent>(
    []( Entity entity,
        TransformComponent& transform,
        const PhysicsComponent& physics )
    {
        physics.mPhysicsObject.GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Call scripts
    mScene.ForEach<StateComponent, ScriptComponent>( 
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

void Engine::PropagateTransforms( Scene& scene )
{
    // Update camera position and orientation from TransformComponent
    scene.ForEach<CameraComponent, TransformComponent>(
    []( Entity entity,
        CameraComponent& camera,
        const TransformComponent& transform )
    {
        // Update position
        camera.mPosition = transform.mPosition;

        // Convert quaternion rotation to yaw/pitch
        vec3 eulerAngles = transform.mRotation;
        camera.mPitch = eulerAngles.x;
        camera.mYaw = eulerAngles.y;

        // Update camera vectors from yaw/pitch
        camera.EulerAnglesToVectors();
    } );

    // Update light position and direction from TransformComponent
    scene.ForEach<LightComponent, TransformComponent>(
    []( Entity entity,
        LightComponent& light,
        const TransformComponent& transform )
    {
        // Update position
        light.mPosition = transform.mPosition;
        // Update direction from rotation (forward is down in local space)
        light.mDirection = transform.RotationMat() * vec4( 0, -1, 0, 0 );
        // Update attenuation constants
        light.Update();
    } );
}

void Engine::DrawScene( Framebuffer& framebuffer )
{
    DrawScene( framebuffer, mScene );
}

void Engine::DrawScene( Framebuffer& framebuffer, Scene& scene )
{
    // Set up lights
    std::vector<Light> lights;
    scene.ForEach<TransformComponent, LightComponent>(
        [&]( const Entity _,
             const TransformComponent& transformComponent,
             const LightComponent& lightComponent )
    {
        lights.push_back( (Light)lightComponent );
    } );

    if ( lights.size() > Renderer::cMaxLights )
        throw std::runtime_error( std::format( "Max lights overflow {}/{}", lights.size(), Renderer::cMaxLights ) );

    // Set up viewport
    framebuffer.Bind();
    mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );

    mRenderer.SetCameraUniformBuffers( mActiveCamera, framebuffer );
    mRenderer.SetLightsUniformBuffer( mActiveCamera, lights );

    // Render models
    scene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
        [&]( const Entity _,
             const ModelComponent& modelComponent,
             const ShaderComponent& shaderComponent,
             const TransformComponent& transformComponent )
    {
        const bool valid = modelComponent.mModel and shaderComponent.mShader;
        const auto& model = valid ? modelComponent.mModel : mErrorModel;
        const auto& shader = valid ? shaderComponent.mShader : mWhiteShader;
        const auto tansform = valid ? transformComponent.TransformMat() : transformComponent.TranslationRotationMat();
        mRenderer.DrawModel( model, shader, tansform );
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


void Engine::DrawBillboard( const Ref<Texture2D>& texture,
                              const Ref<Shader>& shader,
                              const vec3& position,
                              const vec2& size,
                              const vec4& tintColor )
{
    if ( not texture || not shader )
    {
        BUBBLE_ASSERT( false, "DrawBillboard: Texture or shader is null" );
        return;
    }

    // Set shader uniforms
    shader->SetUni3f( "uBillboardPos", position );
    shader->SetUni2f( "uBillboardSize", size );
    shader->SetUni4f( "uTintColor", tintColor );
    shader->SetUni1i( "uTexture", 0 );

    // Bind texture
    texture->Bind( 0 );

    // Draw billboard quad
    mRenderer.DrawMesh( *mBillboardQuad, shader, glm::identity<mat4>() );
}


const Ref<Texture2D>& Engine::GetLightTexture( const LightType& lightType )
{
    switch ( lightType )
    {
        case LightType::Point:
            return mScenePointLightTexture;
        case LightType::Spot:
            return mSceneSpotLightTexture;
        case LightType::Directional:
            return mSceneDirLightTexture;
    }
    BUBBLE_ASSERT( false, "Unknown light type" );
    return mSceneCameraTexture;
}


void Engine::DrawEditorBillboards( Framebuffer& framebuffer, Scene& scene )
{
    framebuffer.Bind();

    // Camera icons (billboards)
    scene.ForEach<CameraComponent, TransformComponent>(
        [&]( const Entity entity,
             const CameraComponent& cameraComponent,
             const TransformComponent& transformComponent )
    {
        DrawBillboard(
            mSceneCameraTexture,
            mBillboardShader,
            transformComponent.mPosition,
            cBillboardSize,
            cBillboardTint
        );
    } );

    // Light icons (billboards)
    scene.ForEach<LightComponent, TransformComponent>(
        [&]( const Entity entity,
             const LightComponent& lightComponent,
             const TransformComponent& transformComponent )
    {
        const auto& lightTexture = GetLightTexture( lightComponent.mType );
        DrawBillboard(
            lightTexture,
            mBillboardShader,
            transformComponent.mPosition,
            cBillboardSize,
            cBillboardTint
        );
    } );
}


void Engine::DrawBillboardEntityId( const Entity entity,
                                      const vec3& position,
                                      const vec2& size )
{
    if ( not mEntityIdBillboardShader )
    {
        BUBBLE_ASSERT( false, "DrawBillboardEntityId: shader is null" );
        return;
    }

    // Set shader uniforms
    mEntityIdBillboardShader->SetUni1u( "uObjectId", (u32)entity );
    mEntityIdBillboardShader->SetUni3f( "uBillboardPos", position );
    mEntityIdBillboardShader->SetUni2f( "uBillboardSize", size );

    // Draw billboard quad
    mRenderer.DrawMesh( *mBillboardQuad, mEntityIdBillboardShader, glm::identity<mat4>() );
}


void Engine::DrawEntityIds( Framebuffer& framebuffer, Scene& scene )
{
    // Draw scene's entity ids to buffer
    framebuffer.Bind();
    mRenderer.ClearScreenUint( uvec4( 0 ) );


    // Draw 3D models
    scene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
        [&]( const Entity entity,
             const ModelComponent& modelComponent,
             const ShaderComponent& shaderComponent,
             const TransformComponent& transformComponent )
    {
        mEntityIdShader->SetUni1u( "uObjectId", (u32)entity );
        const bool valid = modelComponent.mModel and shaderComponent.mShader;
        const auto& model = valid ? modelComponent.mModel : mErrorModel;
        const auto tansform = valid ? transformComponent.TransformMat() : transformComponent.TranslationRotationMat();
        mRenderer.DrawModel( model, mEntityIdShader, tansform );
    } );


    // Draw camera billboards
    scene.ForEach<CameraComponent, TransformComponent>(
        [&]( const Entity entity,
             const CameraComponent& cameraComponent,
             const TransformComponent& transformComponent )
    {
        DrawBillboardEntityId( entity, transformComponent.mPosition, cBillboardSize );
    } );

    // Draw light billboards
    scene.ForEach<LightComponent, TransformComponent>(
        [&]( const Entity entity,
             const LightComponent& lightComponent,
             const TransformComponent& transformComponent )
    {
        DrawBillboardEntityId( entity, transformComponent.mPosition, cBillboardSize );
    } );
}

} // namespace bubble
