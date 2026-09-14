#pragma once
#include "engine/window/window.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include "engine/renderer/renderer.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/audio/audio_engine.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
class Scene;

struct Engine
{
    Engine( Window& window );
    ~Engine();

    // A run: open the project, bind the Lua VM once, then load the startup
    // level - or levelRel (relative to the project root) if given.
    void OnStart( const path& project, const path& levelRel = {} );
    void OnEnd();
    void OnUpdate();

    // A level: what a run holds between two switches. The VM, the loader and
    // global_state are the run's; bodies, voices, the active camera and the
    // scene are the level's and go with it.
    void LoadLevel( const path& relFile );
    void UnloadLevel();
    
    // The direction of the arrow is what decides where a pass runs in the frame.
    // Anything ending at a transform is an input to gameplay and runs before the
    // scripts; anything starting at one is a consumer and runs after them, since
    // a script is free to move a transform or create an entity outright.

    // physics -> transform component. Before the scripts.
    void PropagatePhysicsTransforms( Scene& scene );
    // transform component -> the position AudioSourceComponent::Play() starts a
    // voice at. Before the scripts, because a script calling play() has to get
    // the position the entity is at now.
    void PropagateAudioSourcePositions( Scene& scene );
    // transform component -> camera. After the scripts, and before the active
    // camera sync that reads the CameraComponent this writes.
    void PropagateCameraTransforms( Scene& scene );
    // transform component -> light. After the scripts. DrawScene derives what
    // it renders from the transform directly, so this is what keeps the
    // component consistent for the inspector, serialization and billboards.
    void PropagateLightTransforms( Scene& scene );
    // transform component -> audio listener and playing voices. After the
    // scripts, and after the active camera sync - the fallback listener is the
    // camera, and it should be this frame's.
    void PropagateAudioTransforms( Scene& scene );

    void DrawScene( Framebuffer& framebuffer );
    void DrawScene( Framebuffer& framebuffer, const Scene& scene );

    // Helpers
    void DrawBoundingBoxes( Framebuffer& framebuffer, const Scene& scene );
    void DrawPhysicsShapes( Framebuffer& framebuffer, const Scene& scene );
    void DrawCameraFrustums( Framebuffer& framebuffer, const Scene& scene );

    // Billboards
    static constexpr auto cBillboardSize = vec2( 5.0f );
    static constexpr auto cBillboardTint = vec4( 1.0f );
    const Ref<Texture2D>& GetLightTexture( const LightType& lightType );
    // Draws into an already open pass, so it takes a target rather than a
    // framebuffer - several billboards share one pass.
    void DrawBillboard( const RenderTarget& target,
                        const Ref<Texture2D>& texture,
                        const Ref<Shader>& shader,
                        const vec3& position,
                        const vec2& size = vec2( 1.0f ),
                        const vec4& tintColor = vec4( 1.0f ),
                        u32 objectId = 0 );
    void DrawEditorBillboards( Framebuffer& framebuffer, const Scene& scene );

    // Draw entity id to framebuffer for object picking
    void DrawEntityIds( Framebuffer& framebuffer, const Scene& scene );

public:
    Window& mWindow;
    Timer mTimer;
    Renderer mRenderer;
    PhysicsEngine mPhysicsEngine;
    AudioEngine mAudioEngine;
    Project mProject;

    // Engine
    Camera mCamera;
    Entity mActiveCameraEntity = INVALID_ENTITY;

    // Set by load_level from a script, applied at the end of OnUpdate: a switch
    // in the middle of the script loop would destroy the pools it walks.
    opt<path> mPendingLevel;

    // Shaders for entity ID rendering
    Ref<Shader> mEntityIdShader;
    Ref<Shader> mEntityIdBillboardShader;

    // Visualization boxes and physics shapes
    Ref<Shader> mWhiteShader;

    // Draws a model whose entity has no ShaderComponent, so that adding a model
    // is enough to see something.
    Ref<Shader> mDefaultShader;

    // Reused between draws so packing an entity's uniforms does not allocate
    // once per model per frame.
    vector<u8> mUserUniformScratch;

    // The entities whose scripts run this frame, taken before the first one is
    // called - see the comment in OnUpdate. A member rather than a local so the
    // frame does not allocate a vector per tick.
    vector<Entity> mScriptEntities;

    // Visualization Bounding boxes and Physics shapes
    struct MeshHelpers
    {
        VertexBufferData mVertices;
        vector<u32> mIndices;
        Mesh mMesh;
    };
    MeshHelpers mBoundingBoxes;
    MeshHelpers mPhysicsShapes;
    MeshHelpers mCameraFrustums;

    // Visualization Camera, Lights billboards
    Ref<Mesh> mBillboardQuad;
    Ref<Shader> mBillboardShader;
    Ref<Texture2D> mSceneCameraTexture;
    Ref<Texture2D> mScenePointLightTexture;
    Ref<Texture2D> mSceneSpotLightTexture;
    Ref<Texture2D> mSceneDirLightTexture;
    Ref<Texture2D> mSceneAudioTexture;

    // Error values
    Ref<Texture2D> mErrorTexture;
    Ref<Model> mErrorModel;
};

}
