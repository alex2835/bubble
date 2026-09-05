#include "engine/pch/pch.hpp"
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/renderer/helpers/create_billboard.hpp"
#include "engine/types/any.hpp"
#include "engine/renderer/gpu_context.hpp"
#include <sol/sol.hpp>

namespace bubble
{
Engine::Engine( Window& window )
    : mWindow( window ),
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),
      mEntityIdBillboardShader( LoadShader( ENTITY_PICKING_BILLBOARD_SHADER ) ),

      mWhiteShader( LoadShader( WHITE_SHADER ) ),
      mDefaultShader( LoadShader( PHONG_SHADER ) ),
      mBoundingBoxes{ .mMesh=Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },
      mPhysicsShapes{ .mMesh=Mesh( "Physics", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },
      mCameraFrustums{ .mMesh=Mesh( "CameraFrustum", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) },

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
}

Engine::~Engine()
{
    mPhysicsEngine.ClearWorld();
}

void Engine::OnStart( const path& projectRootFile )
{
    mProject.mScriptingEngine.SetCurrentState();
    mProject.Open( projectRootFile );

    // Bind members to scripting engine
    mProject.mScriptingEngine.BindWindow( mWindow );
    mProject.mScriptingEngine.BindInput( mWindow.GetWindowInput() );
    mProject.mScriptingEngine.BindLoader( mProject.mLoader );
    mProject.mScriptingEngine.BindScene( mProject.mScene, mPhysicsEngine );
    mProject.mScriptingEngine.BindTimer( mTimer );


    // Add RigidBody components to physics world
    mProject.mScene.ForEach<TransformComponent, RigidBodyComponent>(
    [&]( Entity entity, TransformComponent& transform, RigidBodyComponent& rigidBody )
    {
        rigidBody.mRigidBody.SetTransform( transform.mPosition, transform.mRotation );
        rigidBody.mRigidBody.ClearForces();
        mPhysicsEngine.Add( rigidBody.mRigidBody, entity );
    } );

    // Add CharacterController components to physics world
    mProject.mScene.ForEach<TransformComponent, CharacterControllerComponent>(
    [&]( Entity entity, TransformComponent& transform, CharacterControllerComponent& controller )
    {
        controller.mController.Warp( transform.mPosition );
        mPhysicsEngine.Add( controller.mController, entity );
    } );

    /// Scripts
    // A script's callbacks are handed the entity's state table, so every loop
    // below pairs ScriptComponent with StateComponent - and an entity that had
    // a script and no state simply never ran, silently. Giving it an empty one
    // is what the user meant: add_script alone should work, and `state.foo = 1`
    // in on_start should be all it takes to start using it.
    {
        vector<Entity> needState;
        mProject.mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& )
        {
            if ( not mProject.mScene.HasComponent<StateComponent>( entity ) )
                needState.push_back( entity );
        } );
        // Added outside the iteration: AddComponent grows the state pool, and
        // growing one pool while ForEach walks another is not worth relying on.
        for ( Entity entity : needState )
            mProject.mScene.AddComponent<StateComponent>( entity );
    }

    // Extract scripts functions
    mProject.mScene.ForEach<ScriptComponent, StateComponent>( [&]( Entity entity, 
                                                                   ScriptComponent& scriptComponent,
                                                                   StateComponent& stateComponent )
    {
        if ( not scriptComponent.mScript )
            throw std::runtime_error( std::format( "Entity {} has a ScriptComponent with no script assigned",
                                                   (u64)entity ) );

        auto callbacks = mProject.mScriptingEngine.ExtractCallbacks( scriptComponent.mScript );
        scriptComponent.mOnStart = std::move( callbacks.mOnStart );
        scriptComponent.mOnUpdate = std::move( callbacks.mOnUpdate );
        BUBBLE_ASSERT( stateComponent.mState->as<Table>().lua_state() == scriptComponent.mOnUpdate.lua_state(), "Lua state missmatch" );
    } );
    mProject.mScriptingEngine.SetVar( "global_state"sv, *mProject.mGlobalState );

    // Active camera control
    mProject.mScriptingEngine.SetVar( "set_active_camera"sv, [&]( Entity entity ) { mActiveCameraEntity = entity; } );
    mProject.mScriptingEngine.SetVar( "get_active_camera"sv, [&]() -> Entity { return mActiveCameraEntity; } );

    // on_start runs only once every script has been extracted and every global
    // is in place, so the first script to start already sees the whole API and
    // whatever the others put in global_state.
    mProject.mScene.ForEach<StateComponent, ScriptComponent>(
    []( Entity entity,
        const StateComponent& stateComponent,
        const ScriptComponent& scriptComponent )
    {
        CallScriptOnStart( scriptComponent.mOnStart, scriptComponent.mScript,
                           entity, *stateComponent.mState );
    } );

    // Last, so neither loading the project nor running on_start counts against
    // the clock scripts read, and so the first frame's delta is measured from
    // here rather than from whenever the Engine was constructed.
    mTimer.Reset();
}

void Engine::OnEnd()
{
    // A script is free to lock the cursor and has nowhere to give it back, so
    // the engine hands it back on the way out.
    mWindow.LockCursor( false );

    mPhysicsEngine.ClearWorld();
    mPhysicsEngine = PhysicsEngine();
    mProject.mScene = Scene();
    mProject.mLoader = Loader();
    mProject.mGlobalState.reset();
    mProject.mScriptingEngine = ScriptingEngine();
}

void Engine::OnUpdate() 
{
    mTimer.OnUpdate();
    const auto dt = mTimer.GetDeltaTime();

    /// Update physics world
    mPhysicsEngine.Update( dt );

    // Propagate transforms
    PropagatePhysicsTransforms( mProject.mScene );
    PropagateTransforms( mProject.mScene );

    /// Update Scripts
    const f32 deltaSeconds = dt.Seconds();

    // Call scripts
    mProject.mScene.ForEach<StateComponent, ScriptComponent>(
    [deltaSeconds]( Entity entity,
        const StateComponent& stateComponent,
        const ScriptComponent& scriptComponent )
    {
        if ( scriptComponent.mOnUpdate )
        {
            sol::protected_function_result result =
                scriptComponent.mOnUpdate( entity, *stateComponent.mState, deltaSeconds );
            if ( !result.valid() )
            {
                const sol::error err = result;
                const string name = scriptComponent.mScript ? scriptComponent.mScript->mName
                                                            : string( "<unknown>" );
                const string path = scriptComponent.mScript ? scriptComponent.mScript->mPath.string()
                                                            : string( "<no path>" );
                throw std::runtime_error( std::format( "Script '{}' failed on entity {}.\n  {}\n  {}",
                                                       name, (u64)entity, err.what(), path ) );
            }
        }
    });

    /// Sync active camera entity to rendering camera
    if ( mActiveCameraEntity != INVALID_ENTITY and
         mProject.mScene.HasComponent<CameraComponent>( mActiveCameraEntity ) )
    {
        mCamera = mProject.mScene.GetComponent<CameraComponent>( mActiveCameraEntity );
    }
}

void Engine::PropagatePhysicsTransforms( Scene& scene )
{
    // Update transforms from RigidBody components
    mProject.mScene.ForEach<TransformComponent, RigidBodyComponent>(
        []( Entity entity,
            TransformComponent& transform,
            const RigidBodyComponent& rigidBody )
    {
        rigidBody.mRigidBody.GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Update transforms from CharacterController components
    scene.ForEach<TransformComponent, CharacterControllerComponent>(
        []( Entity entity,
            TransformComponent& transform,
            const CharacterControllerComponent& controller )
    {
        transform.mPosition = controller.mController.GetPosition();
    } );
}

void Engine::PropagateTransforms( Scene& scene )
{
    // Update camera position and orientation from TransformComponent (free cameras only)
    scene.ForEach<CameraComponent, TransformComponent>(
    []( Entity entity,
        CameraComponent& camera,
        const TransformComponent& transform )
    {
        if ( !camera.mUseTransformPropagation )
            return;

        camera.mPosition = transform.mPosition;
        camera.mYaw      = transform.mRotation.y;
        camera.mPitch    = transform.mRotation.x;
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

// Every draw entry point below records and submits its own command buffer.
//
// OpenGL had one implicit framebuffer binding and a glClear call, so these
// functions could each just Bind() and draw. WebGPU makes the target explicit:
// a pass names its attachments and says up front whether it clears or loads.
// DrawScene clears, everything after it loads, which is what keeps the helper
// overlays from wiping the scene they are drawn on top of.

namespace
{
// Records one pass and submits it. Keeping this in one place means every entry
// point flushes the draw uniform ring before submitting, which is easy to
// forget and shows up as stale transforms rather than as an error.
template <typename RecordFn>
void SubmitPass( Renderer& renderer, string_view label, RecordFn&& record )
{
    wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
    encoderDesc.label = wgpu::StringView( label );
    wgpu::raii::CommandEncoder encoder( Gpu().Device().createCommandEncoder( encoderDesc ) );

    record( *encoder );

    renderer.FlushDrawUniforms();

    wgpu::CommandBufferDescriptor cmdDesc = wgpu::Default;
    cmdDesc.label = wgpu::StringView( label );
    wgpu::raii::CommandBuffer commands( encoder->finish( cmdDesc ) );
    wgpu::CommandBuffer raw = *commands;
    Gpu().Queue().submit( 1, &raw );
}
}


void Engine::DrawScene( Framebuffer& framebuffer )
{
    DrawScene( framebuffer, mProject.mScene );
}


void Engine::DrawScene( Framebuffer& framebuffer, const Scene& scene )
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

    mRenderer.SetCameraUniformBuffers( mCamera, framebuffer );
    mRenderer.SetLightsUniformBuffer( mCamera, lights );
    mRenderer.FlushFrameUniforms();

    SubmitPass( mRenderer, "Scene", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, vec4( 0.2f, 0.3f, 0.3f, 1.0f ), true, "Scene" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );

        // Render models. Iterating on ModelComponent and TransformComponent only:
        // a model with no ShaderComponent used to be skipped by the ForEach and
        // never drawn at all, with nothing logged - the single most confusing way
        // for a first entity to come out invisible. It gets the default shader.
        scene.ForEach<ModelComponent, TransformComponent>(
            [&]( const Entity entity,
                 const ModelComponent& modelComponent,
                 const TransformComponent& transformComponent )
        {
            if ( not modelComponent.mModel )
            {
                mRenderer.DrawModel( target, mErrorModel, mWhiteShader,
                                     transformComponent.TranslationRotationMat() );
                return;
            }

            const ShaderComponent* shaderComponent =
                scene.HasComponent<ShaderComponent>( entity )
                ? &scene.GetComponent<ShaderComponent>( entity )
                : nullptr;

            const Ref<Shader>& shader = shaderComponent and shaderComponent->mShader
                                        ? shaderComponent->mShader
                                        : mDefaultShader;
            if ( not shader )
            {
                mRenderer.DrawModel( target, mErrorModel, mWhiteShader,
                                     transformComponent.TranslationRotationMat() );
                return;
            }

            // The entity's own uniform values, packed into the block its
            // shader declares and staged for the draw about to be recorded.
            if ( shaderComponent and shaderComponent->mUniforms and
                 shaderComponent->mUniforms->is<Table>() and shader->mUserUniformSize > 0 )
            {
                PackShaderUniforms( *shader, shaderComponent->mUniforms->as<Table>(),
                                    mUserUniformScratch );
                mRenderer.SetUserUniforms( mUserUniformScratch.data(),
                                           mUserUniformScratch.size() );
            }

            mRenderer.DrawModel( target, modelComponent.mModel, shader,
                                 transformComponent.TransformMat() );
        } );
    } );
}


void Engine::DrawBoundingBoxes( Framebuffer& framebuffer, const Scene& scene )
{
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

    if ( mBoundingBoxes.mIndices.empty() )
        return;

    mBoundingBoxes.mMesh.UpdateDynamicVertexBufferData( mBoundingBoxes.mVertices, mBoundingBoxes.mIndices );

    SubmitPass( mRenderer, "Bounding Boxes", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, std::nullopt, false, "Bounding Boxes" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );
        mRenderer.DrawMesh( target, mBoundingBoxes.mMesh, mWhiteShader,
                            glm::identity<mat4>(), DrawingPrimitive::Lines );
    } );
}


void Engine::DrawPhysicsShapes( Framebuffer& framebuffer, const Scene& scene )
{
    if ( scene.Size() == 0 )
        return;

    u32 elementIndexStride = 0;
    mPhysicsShapes.mVertices.Clear();
    mPhysicsShapes.mIndices.clear();

    // Draw RigidBody shapes
    scene.ForEach<RigidBodyComponent, TransformComponent>(
        [&]( Entity _,
             const RigidBodyComponent& rigidBody,
             const TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();
        const auto& [vertices, indices] = rigidBody.mRigidBody.GetShapeData();
        for ( auto vertex : vertices )
            mPhysicsShapes.mVertices.mPositions.push_back( vec3( trans * vec4( vertex, 1 ) ) );
        for ( u32 index : indices )
            mPhysicsShapes.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mPhysicsShapes.mVertices.mPositions.size();
    } );

    // Draw CharacterController shapes
    scene.ForEach<CharacterControllerComponent, TransformComponent>(
        [&]( Entity _,
             const CharacterControllerComponent& controller,
             const TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();
        const auto& [vertices, indices] = controller.mController.GetShapeData();
        for ( auto vertex : vertices )
            mPhysicsShapes.mVertices.mPositions.push_back( vec3( trans * vec4( vertex, 1 ) ) );
        for ( u32 index : indices )
            mPhysicsShapes.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mPhysicsShapes.mVertices.mPositions.size();
    } );

    if ( mPhysicsShapes.mIndices.empty() )
        return;

    mPhysicsShapes.mMesh.UpdateDynamicVertexBufferData( mPhysicsShapes.mVertices, mPhysicsShapes.mIndices );

    SubmitPass( mRenderer, "Physics Shapes", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, std::nullopt, false, "Physics Shapes" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );
        mRenderer.DrawMesh( target, mPhysicsShapes.mMesh, mWhiteShader,
                            glm::identity<mat4>(), DrawingPrimitive::Lines );
    } );
}


void Engine::DrawCameraFrustums( Framebuffer& framebuffer, const Scene& scene )
{
    if ( scene.Size() == 0 )
        return;

    u32 elementIndexStride = 0;
    mCameraFrustums.mVertices.Clear();
    mCameraFrustums.mIndices.clear();

    // Get framebuffer aspect ratio for frustum calculation
    f32 aspectRatio = (f32)framebuffer.Width() / (f32)framebuffer.Height();

    scene.ForEach<CameraComponent, TransformComponent>(
        [&]( Entity _,
             const CameraComponent& camera,
             const TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();
        const f32 cameraFarPlane = camera.mNear + 20.0f;
        const auto& [vertices, indices] = GenerateFrustumLinesShape( camera.mFov, aspectRatio, camera.mNear, cameraFarPlane );

        for ( auto vertex : vertices )
            mCameraFrustums.mVertices.mPositions.push_back( vec3( trans * vec4( vertex, 1 ) ) );
        for ( u32 index : indices )
            mCameraFrustums.mIndices.push_back( index + elementIndexStride );
        elementIndexStride = (u32)mCameraFrustums.mVertices.mPositions.size();
    } );

    if ( mCameraFrustums.mIndices.empty() )
        return;

    mCameraFrustums.mMesh.UpdateDynamicVertexBufferData( mCameraFrustums.mVertices, mCameraFrustums.mIndices );

    SubmitPass( mRenderer, "Camera Frustums", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, std::nullopt, false, "Camera Frustums" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );
        mRenderer.DrawMesh( target, mCameraFrustums.mMesh, mWhiteShader,
                            glm::identity<mat4>(), DrawingPrimitive::Lines );
    } );
}


void Engine::DrawBillboard( const RenderTarget& target,
                            const Ref<Texture2D>& texture,
                            const Ref<Shader>& shader,
                            const vec3& position,
                            const vec2& size,
                            const vec4& tintColor,
                            u32 objectId )
{
    if ( not shader )
    {
        BUBBLE_ASSERT( false, "DrawBillboard: shader is null" );
        return;
    }

    // The billboard's texture rides in through the material bind group rather
    // than one of its own: a billboard is a quad with exactly one map, which is
    // what the material group already describes. The quad is shared, so its
    // diffuse map is swapped per draw and the cached bind group dropped.
    if ( texture and mBillboardQuad->mMaterial.mDiffuseMap != texture )
    {
        mBillboardQuad->mMaterial.mDiffuseMap = texture;
        mBillboardQuad->mMaterial.InvalidateBindGroup();
    }

    DrawUniforms extras;
    extras.mBillboardPos = vec4( position, 0.0f );
    extras.mBillboardSize = vec4( size, 0.0f, 0.0f );
    extras.mTintColor = tintColor;

    mRenderer.DrawMesh( target, *mBillboardQuad, shader, glm::identity<mat4>(),
                        DrawingPrimitive::Triangles, objectId, &extras );
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


void Engine::DrawEditorBillboards( Framebuffer& framebuffer, const Scene& scene )
{
    SubmitPass( mRenderer, "Editor Billboards", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, std::nullopt, false, "Editor Billboards" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );

        // Camera icons (billboards)
        scene.ForEach<CameraComponent, TransformComponent>(
            [&]( const Entity entity,
                 const CameraComponent& cameraComponent,
                 const TransformComponent& transformComponent )
        {
            DrawBillboard( target, mSceneCameraTexture, mBillboardShader,
                           transformComponent.mPosition, cBillboardSize, cBillboardTint );
        } );

        // Light icons (billboards)
        scene.ForEach<LightComponent, TransformComponent>(
            [&]( const Entity entity,
                 const LightComponent& lightComponent,
                 const TransformComponent& transformComponent )
        {
            const auto& lightTexture = GetLightTexture( lightComponent.mType );
            DrawBillboard( target, lightTexture, mBillboardShader,
                           transformComponent.mPosition, cBillboardSize, cBillboardTint );
        } );
    } );
}


void Engine::DrawEntityIds( Framebuffer& framebuffer, const Scene& scene )
{
    // The camera block is whatever DrawScene left in it. That was true under
    // OpenGL too, and holds as long as this runs after DrawScene in the same
    // frame - which the editor's on demand scheduling preserves.
    SubmitPass( mRenderer, "Entity Ids", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPassUint( encoder, uvec4( 0 ), true, "Entity Ids" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );

        // Draw 3D models. Matches DrawScene: whatever is visible there has to be
        // pickable here, and a model drawn with the default shader has no
        // ShaderComponent to iterate on.
        scene.ForEach<ModelComponent, TransformComponent>(
            [&]( const Entity entity,
                 const ModelComponent& modelComponent,
                 const TransformComponent& transformComponent )
        {
            const bool valid = modelComponent.mModel != nullptr;
            const auto& model = valid ? modelComponent.mModel : mErrorModel;
            const auto tansform = valid ? transformComponent.TransformMat()
                                        : transformComponent.TranslationRotationMat();
            mRenderer.DrawModel( target, model, mEntityIdShader, tansform,
                                 DrawingPrimitive::Triangles, (u32)entity );
        } );

        // Draw camera billboards
        scene.ForEach<CameraComponent, TransformComponent>(
            [&]( const Entity entity,
                 const CameraComponent& cameraComponent,
                 const TransformComponent& transformComponent )
        {
            DrawBillboard( target, nullptr, mEntityIdBillboardShader,
                           transformComponent.mPosition, cBillboardSize,
                           vec4( 1.0f ), (u32)entity );
        } );

        // Draw light billboards
        scene.ForEach<LightComponent, TransformComponent>(
            [&]( const Entity entity,
                 const LightComponent& lightComponent,
                 const TransformComponent& transformComponent )
        {
            DrawBillboard( target, nullptr, mEntityIdBillboardShader,
                           transformComponent.mPosition, cBillboardSize,
                           vec4( 1.0f ), (u32)entity );
        } );
    } );
}

} // namespace bubble
