#include "engine/pch/pch.hpp"
#include "engine/engine.hpp"
#include "engine/project/project.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/renderer/helpers/create_billboard.hpp"
#include "engine/types/any.hpp"
#include "engine/renderer/shader_uniforms.hpp"
#include "engine/renderer/gpu_context.hpp"
#include <sol/sol.hpp>
#include "engine/scene/components/audio_source_component.hpp"
#include "engine/scene/components/character_controller_component.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/animator_component.hpp"
#include "engine/animation/animator.hpp"
#include <ozz/animation/runtime/skeleton.h>
#include "engine/scene/components/script_component.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/components/audio_listener_component.hpp"
#include "engine/scene/components/camera_component.hpp"
#include "engine/scene/components/light_component.hpp"
#include "engine/scene/components/model_component.hpp"
#include "engine/scene/components/shader_component.hpp"

namespace bubble
{
namespace
{
// The joint matrices to draw the entity's model with, if it is animated.
// Empty until UpdateAnimations has run for it, and the model then draws at
// rest.
std::span<const mat4> SkinOf( const Scene& scene, Entity entity )
{
    if ( not scene.HasComponent<AnimatorComponent>( entity ) )
        return {};
    const Animator* animator = scene.GetComponent<AnimatorComponent>( entity ).mAnimator.get();
    return animator ? animator->SkinMatrices() : std::span<const mat4>{};
}
}

Engine::Engine( Window& window )
    : mWindow( window ),
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),
      mEntityIdBillboardShader( LoadShader( ENTITY_PICKING_BILLBOARD_SHADER ) ),

      mWhiteShader( LoadShader( WHITE_SHADER ) ),
      mDefaultShader( LoadShader( PHONG_SHADER ) ),
      mBoundingBoxes{ .mMesh=Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{} ) },
      mPhysicsShapes{ .mMesh=Mesh( "Physics", BasicMaterial(), VertexBufferData{}, vector<u32>{} ) },
      mSkeletons{ .mMesh=Mesh( "Skeletons", BasicMaterial(), VertexBufferData{}, vector<u32>{} ) },
      mCameraFrustums{ .mMesh=Mesh( "CameraFrustum", BasicMaterial(), VertexBufferData{}, vector<u32>{} ) },

      // billboards
      mBillboardShader( LoadShader( BILBOARD_SHADER ) ),
      mBillboardQuad( CreateBillboardQuadMesh() ),
      mSceneCameraTexture( LoadTexture2D( SCENE_CAMERA_TEXTURE ) ),
      mScenePointLightTexture( LoadTexture2D( SCENE_POINT_LIGHT_TEXTURE ) ),
      mSceneSpotLightTexture( LoadTexture2D( SCENE_SPOT_LIGHT_TEXTURE ) ),
      mSceneDirLightTexture( LoadTexture2D( SCENE_DIR_LIGHT_TEXTURE ) ),
      mSceneAudioTexture( LoadTexture2D( SCENE_AUDIO_TEXTURE ) ),

      // Error values
      mErrorModel( LoadModel( ERROR_MODEL ) ),
      mErrorTexture( LoadTexture2D( ERROR_TEXTURE ) )
{
}

Engine::~Engine()
{
    mPhysicsEngine.ClearWorld();
}

void Engine::OnStart( const path& projectRootFile, const path& levelRel )
{
    mProject.mScriptingEngine.SetCurrentState();
    mProject.Open( projectRootFile, /*openStartupLevel*/ false );

    // Bind members to scripting engine. Once per run: the VM, and everything
    // it closes over, outlives every level. BindScene takes mLevel.mScene by
    // reference, and a level switch loads into that same object.
    mProject.mScriptingEngine.BindWindow( mWindow );
    mProject.mScriptingEngine.BindInput( mWindow.GetWindowInput() );
    mProject.mScriptingEngine.BindLoader( mProject.mLoader );
    mProject.mScriptingEngine.BindScene( mProject.mLevel.mScene, mPhysicsEngine );
    mProject.mScriptingEngine.BindTimer( mTimer );
    mProject.mScriptingEngine.SetVar( "global_state"sv, *mProject.mGlobalState );

    // Active camera control
    mProject.mScriptingEngine.SetVar( "set_active_camera"sv, [&]( Entity entity ) { mActiveCameraEntity = entity; } );
    mProject.mScriptingEngine.SetVar( "get_active_camera"sv, [&]() -> Entity { return mActiveCameraEntity; } );

    // Levels. load_level only records the request: it is called from inside a
    // script, which is inside the walk over the pools that switching would
    // destroy. The switch happens at the end of OnUpdate. The file is checked
    // here so a typo fails in the script that made it, with its name attached.
    mProject.mScriptingEngine.SetVar( "load_level"sv, [&]( const string& relFile )
    {
        if ( not filesystem::is_regular_file( mProject.RootDir() / relFile ) )
            throw std::runtime_error( std::format( "load_level: no such level '{}'", relFile ) );
        mPendingLevel = relFile;
    } );
    mProject.mScriptingEngine.SetVar( "current_level"sv, [&]() -> string
    {
        return mProject.CurrentLevel().generic_string();
    } );

    // The startup level, unless the caller picked one - the editor runs
    // whatever level it has open.
    LoadLevel( levelRel.empty() ? mProject.mStartupLevel : levelRel );

    // Last, so neither loading the project nor running on_start counts against
    // the clock scripts read, and so the first frame's delta is measured from
    // here rather than from whenever the Engine was constructed.
    mTimer.Reset();
}

void Engine::LoadLevel( const path& relFile )
{
    UnloadLevel();
    mProject.OpenLevel( relFile );

    // Add RigidBody components to physics world
    mProject.mLevel.mScene.ForEach<TransformComponent, RigidBodyComponent>(
    [&]( Entity entity, TransformComponent& transform, RigidBodyComponent& rigidBody )
    {
        rigidBody.mRigidBody.SetTransform( transform.mPosition, transform.mRotation );
        rigidBody.mRigidBody.ClearForces();
        mPhysicsEngine.Add( rigidBody.mRigidBody, entity );
    } );

    // Add CharacterController components to physics world
    mProject.mLevel.mScene.ForEach<TransformComponent, CharacterControllerComponent>(
    [&]( Entity entity, TransformComponent& transform, CharacterControllerComponent& controller )
    {
        controller.mController.Warp( transform.mPosition );
        mPhysicsEngine.Add( controller.mController, entity );
    } );

    // Sources set to play on start. Before on_start runs, so a script that
    // wants to stop one immediately can, and after the scene is fully loaded so
    // every sound asset is resolved.
    mProject.mLevel.mScene.ForEach<AudioSourceComponent>(
    []( Entity entity, AudioSourceComponent& audioSource )
    {
        if ( audioSource.mPlayOnStart )
            audioSource.Play();
    } );

    /// Scripts
    // A script's callbacks are handed the entity's state table, so every loop
    // below pairs ScriptComponent with StateComponent - and an entity that had
    // a script and no state simply never ran, silently. Giving it an empty one
    // is what the user meant: add_script alone should work, and `state.foo = 1`
    // in on_start should be all it takes to start using it.
    {
        vector<Entity> needState;
        mProject.mLevel.mScene.ForEach<ScriptComponent>( [&]( Entity entity, ScriptComponent& )
        {
            if ( not mProject.mLevel.mScene.HasComponent<StateComponent>( entity ) )
                needState.push_back( entity );
        } );
        // Added outside the iteration: AddComponent grows the state pool, and
        // growing one pool while ForEach walks another is not worth relying on.
        for ( Entity entity : needState )
            mProject.mLevel.mScene.AddComponent<StateComponent>( entity );
    }

    // Extract scripts functions
    mProject.mLevel.mScene.ForEach<ScriptComponent, StateComponent>( [&]( Entity entity,
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

    // on_start runs only once every script has been extracted and every global
    // is in place, so the first script to start already sees the whole API and
    // whatever the others put in global_state. An entity created by one of
    // them has already had its own on_start run by spawn, so the snapshot
    // leaving it out is correct.
    ForEachScriptEntity( []( Entity entity, const StateComponent& state, const ScriptComponent& script )
    {
        CallScriptOnStart( script.mOnStart, script.mScript, entity, *state.mState );
    } );
}

void Engine::UnloadLevel()
{
    mPendingLevel.reset();
    // Bodies and voices are owned by the components about to go; the engines
    // would otherwise keep stepping and playing them into the next level.
    //
    // Controllers one by one: a character controller is an *action* in the
    // Bullet world, and ClearWorld only takes out the collision objects. The
    // action would stay registered, pointing at a freed controller, and the
    // next stepSimulation would walk into it.
    mProject.mLevel.mScene.ForEach<CharacterControllerComponent>(
    [&]( Entity, CharacterControllerComponent& controller )
    {
        mPhysicsEngine.Remove( controller.mController );
    } );
    mPhysicsEngine.ClearWorld();
    mAudioEngine.StopAll();
    mActiveCameraEntity = INVALID_ENTITY;
    mMultipleListenersReported = false;
    mProject.mLevel.Clear();
}

void Engine::OnEnd()
{
    // A script is free to lock the cursor and has nowhere to give it back, so
    // the engine hands it back on the way out.
    mWindow.LockCursor( false );

    UnloadLevel();
    mPhysicsEngine = PhysicsEngine();
    mProject.mLoader = Loader();
    mProject.mGlobalState.reset();
    mProject.mScriptingEngine = ScriptingEngine();
}

void Engine::OnUpdate()
{
    mTimer.OnUpdate();
    const auto dt = mTimer.GetDeltaTime();
    const f32 deltaSeconds = dt.Seconds();
    Scene& scene = mProject.mLevel.mScene;

    /// Update physics world
    mPhysicsEngine.Update( dt );

    // Propagations that end at a transform: gameplay inputs, so they run first
    // and a script reads this frame's values.
    PropagatePhysicsTransforms( scene );
    PropagateAudioSourcePositions( scene );

    // Reaping finished voices before the scripts run means is_playing() answers
    // for the frame the script is in, not the one before it. This also drives
    // the web AudioContext resume, which needs a frame that follows a user
    // gesture rather than a specific call site.
    mAudioEngine.OnUpdate();

    UpdateScripts( deltaSeconds );

    // Propagations that start at a transform: consumers, so they run after the
    // scripts. Anything here that ran before them was reading transforms one
    // frame stale, and saw nothing at all of an entity a script had just
    // created - which is how a light spawned from on_update got a frame at the
    // origin with default attenuation.
    PropagateCameraTransforms( scene );
    PropagateLightTransforms( scene );
    UpdateAnimations( scene, deltaSeconds );
    SyncActiveCamera();

    // Last: it reads mCamera for the fallback listener, so it wants the sync
    // above to have happened.
    PropagateAudioTransforms( scene );

    // A level switch a script asked for this frame. Last of all: nothing above
    // may run against a scene that is half unloaded, and the scripts that ran
    // after the one that asked still saw the level they were written for.
    if ( mPendingLevel )
    {
        const path relFile = std::move( *mPendingLevel );
        mPendingLevel.reset();
        LoadLevel( relFile );
    }
}

void Engine::PropagatePhysicsTransforms( Scene& scene )
{
    // Update transforms from RigidBody components
    scene.ForEach<TransformComponent, RigidBodyComponent>(
        []( Entity,
            TransformComponent& transform,
            const RigidBodyComponent& rigidBody )
    {
        rigidBody.mRigidBody.GetTransform( transform.mPosition, transform.mRotation );
    } );

    // Update transforms from CharacterController components
    scene.ForEach<TransformComponent, CharacterControllerComponent>(
        []( Entity,
            TransformComponent& transform,
            const CharacterControllerComponent& controller )
    {
        transform.mPosition = controller.mController.GetPosition();
    } );
}

// Split out of PropagateAudioTransforms because it is the one audio propagation
// that points the other way. AudioSourceComponent::Play() starts a voice at
// mParams.mPosition, so that field is an input to any script calling play() and
// has to be current before the scripts run - otherwise a sound played on a
// just-spawned entity starts at the origin.
void Engine::PropagateAudioSourcePositions( Scene& scene )
{
    scene.ForEach<AudioSourceComponent, TransformComponent>(
    []( Entity,
        AudioSourceComponent& audioSource,
        const TransformComponent& transform )
    {
        audioSource.SyncToTransform( transform );
    } );
}

void Engine::ForEachScriptEntity( const ScriptEntityFn& fn )
{
    Scene& scene = mProject.mLevel.mScene;

    // Over a snapshot of the entities rather than a live walk.
    //
    // ForEach hands its callback references straight into the component pools
    // and walks them by index, so a script that mutates the scene pulls the
    // ground out from under the iteration it is running inside: Pool::Push
    // reallocates and frees the old buffer, and Pool::Remove compacts every
    // pool and shifts every index after the hole. Taking the entity list first
    // and looking each entity up again is what makes spawn() and
    // remove_entity() safe to call from a script.
    //
    // The snapshot is also the definition of which scripts run this pass: an
    // entity created by a script gets its on_start now and its first on_update
    // on the next tick, rather than a partial one in the middle of this one.
    mScriptEntities.clear();
    scene.ForEach<StateComponent, ScriptComponent>(
    [this]( Entity entity, const StateComponent&, const ScriptComponent& )
    {
        mScriptEntities.push_back( entity );
    } );

    for ( const Entity entity : mScriptEntities )
    {
        // An earlier script this pass may have removed the entity, or taken a
        // component off it. Neither is an error - it just has nothing to run.
        if ( not scene.HasEntity( entity ) or
             not scene.HasComponent<StateComponent>( entity ) or
             not scene.HasComponent<ScriptComponent>( entity ) )
            continue;

        // Looked up per entity and never held across a call: any script may
        // have moved both pools since the snapshot was taken.
        fn( entity,
            scene.GetComponent<StateComponent>( entity ),
            scene.GetComponent<ScriptComponent>( entity ) );
    }
}

void Engine::UpdateScripts( f32 deltaSeconds )
{
    ForEachScriptEntity( [&]( Entity entity, const StateComponent& state, const ScriptComponent& script )
    {
        CallScriptOnUpdate( script.mOnUpdate, script.mScript, entity, *state.mState, deltaSeconds );
    } );
}

// The transform is the truth and the camera is a cache of it - see
// CameraComponent. One direction, every frame, for every camera.
void Engine::PropagateCameraTransforms( Scene& scene )
{
    scene.ForEach<CameraComponent, TransformComponent>(
    []( Entity,
        CameraComponent& camera,
        const TransformComponent& transform )
    {
        camera.mPosition = transform.mPosition;
        camera.VectorsFromEuler( transform.mRotation.y, transform.mRotation.x );
    } );
}

// The rendered light is derived from the transform in DrawScene, which does not
// read any of these fields. This keeps the component itself consistent for the
// inspector, for serialization, and for the editor billboards.
void Engine::PropagateLightTransforms( Scene& scene )
{
    scene.ForEach<LightComponent, TransformComponent>(
    []( Entity,
        LightComponent& light,
        const TransformComponent& transform )
    {
        light.SyncToTransform( transform );
    } );
}

void Engine::UpdateAnimations( Scene& scene, f32 deltaSeconds )
{
    scene.ForEach<ModelComponent, AnimatorComponent>(
    [&]( Entity entity, const ModelComponent& modelComponent, AnimatorComponent& animator )
    {
        const mat4 world = scene.HasComponent<TransformComponent>( entity )
                           ? scene.GetComponent<TransformComponent>( entity ).TransformMat()
                           : glm::identity<mat4>();
        animator.Advance( modelComponent.mModel, deltaSeconds, world );
    } );
}


void Engine::SyncActiveCamera()
{
    if ( mActiveCameraEntity != INVALID_ENTITY and
         mProject.mLevel.mScene.HasComponent<CameraComponent>( mActiveCameraEntity ) )
    {
        mCamera = mProject.mLevel.mScene.GetComponent<CameraComponent>( mActiveCameraEntity );
    }
}

void Engine::PropagateAudioTransforms( Scene& scene )
{
    // The first active listener wins. A scene with two of them is an authoring
    // mistake with no sensible resolution - averaging them would be worse than
    // picking one - so it is reported and the rest are ignored.
    bool listenerFound = false;
    scene.ForEach<AudioListenerComponent, TransformComponent>(
    [&]( Entity,
         const AudioListenerComponent& listener,
         const TransformComponent& transform )
    {
        if ( not listener.mActive )
            return;

        if ( listenerFound )
        {
            if ( not mMultipleListenersReported )
            {
                LogWarning( "More than one active AudioListenerComponent in the scene, "
                            "using the first one found." );
                mMultipleListenersReported = true;
            }
            return;
        }
        listenerFound = true;

        // Same euler convention the camera uses, so a listener parented to the
        // player hears what the camera looks at.
        const f32 yaw = transform.mRotation.y;
        const f32 pitch = transform.mRotation.x;
        const vec3 forward = normalize( vec3( cos( yaw ) * cos( pitch ),
                                              sin( pitch ),
                                              sin( yaw ) * cos( pitch ) ) );
        mAudioEngine.SetListener( transform.mPosition, forward, vec3( 0.0f, 1.0f, 0.0f ) );
    } );

    // Without a listener entity the camera is the ear. This is what makes sound
    // work in a scene nobody has authored audio for yet.
    if ( not listenerFound )
        mAudioEngine.SetListener( mCamera.mPosition, mCamera.mForward, mCamera.mWorldUp );

    // Again after the scripts, so a source an entity carried across the frame
    // does not have its voice trail a frame behind the entity.
    PropagateAudioSourcePositions( scene );
}

void Engine::PropagateEditorAudio( Scene& scene )
{
    mAudioEngine.SetListener( mCamera.mPosition, mCamera.mForward, mCamera.mWorldUp );
    PropagateAudioSourcePositions( scene );
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
    DrawScene( framebuffer, mProject.mLevel.mScene );
}


void Engine::DrawScene( Framebuffer& framebuffer, const Scene& scene )
{
    // Set up lights.
    //
    // Position, direction and the attenuation constants are derived here rather
    // than read off the component. PropagateLightTransforms keeps the same fields up
    // to date for the inspector and for serialization, but it runs before the
    // scripts do, so an entity spawned from on_update reached this loop once
    // with all three still at their defaults - every light of that first frame
    // stacked at the origin with an attenuation of 1/1/1, which is a flash.
    // Deriving them from the transform we are already iterating makes a light
    // correct on the frame it is created.
    std::vector<Light> lights;
    scene.ForEach<TransformComponent, LightComponent>(
        [&]( const Entity _,
             const TransformComponent& transformComponent,
             const LightComponent& lightComponent )
    {
        Light& light = lights.emplace_back( (Light)lightComponent );
        light.mPosition = transformComponent.mPosition;
        light.mDirection = transformComponent.RotationMat() * vec4( 0, -1, 0, 0 );
        light.Update();
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
                                 transformComponent.TransformMat(),
                                 DrawingPrimitive::Triangles, 0, SkinOf( scene, entity ) );
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


void Engine::DrawSkeletons( Framebuffer& framebuffer, const Scene& scene )
{
    if ( scene.Size() == 0 )
        return;

    mSkeletons.mVertices.Clear();
    mSkeletons.mIndices.clear();

    scene.ForEach<AnimatorComponent, TransformComponent>(
        [&]( Entity _,
             const AnimatorComponent& animatorComponent,
             const TransformComponent& transform )
    {
        const Animator* animator = animatorComponent.mAnimator.get();
        if ( not animator )
            return;
        const auto joints = animator->JointMatrices();
        const auto parents = animator->GetModel()->mSkeleton->mSkeleton->joint_parents();
        const mat4 trans = transform.TransformMat();

        // Joint positions first, then a line from each joint to its parent.
        // A root has no parent and no line; it still gets a vertex, so the
        // indices below stay the joint indices.
        const u32 first = (u32)mSkeletons.mVertices.mPositions.size();
        for ( const mat4& joint : joints )
            mSkeletons.mVertices.mPositions.push_back( vec3( trans * joint[3] ) );
        for ( size_t j = 0; j < joints.size(); j++ )
        {
            if ( parents[j] < 0 )
                continue;
            mSkeletons.mIndices.push_back( first + (u32)j );
            mSkeletons.mIndices.push_back( first + (u32)parents[j] );
        }
    } );

    if ( mSkeletons.mIndices.empty() )
        return;

    mSkeletons.mMesh.UpdateDynamicVertexBufferData( mSkeletons.mVertices, mSkeletons.mIndices );

    // The depth is cleared first: the bones sit inside the skin that was just
    // drawn over them, and an overlay that loses to it shows nothing.
    SubmitPass( mRenderer, "Skeletons", [&]( wgpu::CommandEncoder encoder )
    {
        auto pass = framebuffer.BeginRenderPass( encoder, std::nullopt, true, "Skeletons" );
        const RenderTarget target = RenderTarget::For( *pass, framebuffer );
        mRenderer.BindFrame( *pass );
        mRenderer.DrawMesh( target, mSkeletons.mMesh, mWhiteShader,
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

        // Audio source icons (billboards)
        scene.ForEach<AudioSourceComponent, TransformComponent>(
            [&]( const Entity entity,
                 const AudioSourceComponent& audioComponent,
                 const TransformComponent& transformComponent )
        {
            DrawBillboard( target, mSceneAudioTexture, mBillboardShader,
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
                                 DrawingPrimitive::Triangles, (u32)entity, SkinOf( scene, entity ) );
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

        // Draw audio source billboards
        scene.ForEach<AudioSourceComponent, TransformComponent>(
            [&]( const Entity entity,
                 const AudioSourceComponent& audioComponent,
                 const TransformComponent& transformComponent )
        {
            DrawBillboard( target, nullptr, mEntityIdBillboardShader,
                           transformComponent.mPosition, cBillboardSize,
                           vec4( 1.0f ), (u32)entity );
        } );
    } );
}

} // namespace bubble
