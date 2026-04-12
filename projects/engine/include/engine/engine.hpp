#pragma once
#include "engine/window/window.hpp"
#include "engine/loader/loader.hpp"
#include "engine/project/project.hpp"
#include "engine/renderer/renderer.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
class Scene;

struct Engine
{
    Engine( Window& window );
    ~Engine();

    void OnStart( const path& project );
    void OnEnd();
    void OnUpdate();
    
    // physics -> transform component
    void PropagatePhysicsTransforms( Scene& scene );
    // transform component -> lights or cameras
    void PropagateTransforms( Scene& scene );

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
    void DrawBillboard( const Ref<Texture2D>& texture,
                        const Ref<Shader>& shader,
                        const vec3& position,
                        const vec2& size = vec2( 1.0f ),
                        const vec4& tintColor = vec4( 1.0f ) );
    void DrawEditorBillboards( Framebuffer& framebuffer, const Scene& scene );

    // Draw entity id to framebuffer for object picking
    void DrawEntityIds( Framebuffer& framebuffer, const Scene& scene );
    void DrawBillboardEntityId( const Entity entity, const vec3& position, const vec2& size = vec2( 1.0f ) );

public:
    Window& mWindow;
    Timer mTimer;
    Renderer mRenderer;
    PhysicsEngine mPhysicsEngine;
    Project mProject;

    // Engine
    Camera mCamera;
    Entity mActiveCameraEntity = INVALID_ENTITY;

    // Shaders for entity ID rendering
    Ref<Shader> mEntityIdShader;
    Ref<Shader> mEntityIdBillboardShader;

    // Visualization boxes and physics shapes
    Ref<Shader> mWhiteShader;

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

    // Error values
    Ref<Texture2D> mErrorTexture;
    Ref<Model> mErrorModel;
};

}
