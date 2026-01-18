#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/renderer.hpp"

namespace bubble
{
class Project;
class Scene;

struct Engine
{
    Engine( Project& project );
    void OnStart();
    void OnEnd();
    void OnUpdate();
    void DrawScene( Framebuffer& framebuffer );

    void DrawScene( Framebuffer& framebuffer, Scene& scene );
    // Helpers
    void DrawBoundingBoxes( Framebuffer& framebuffer, Scene& scene );
    void DrawPhysicsShapes( Framebuffer& framebuffer, Scene& scene );

    // Billboards
    static constexpr auto cBillboardSize = vec2( 5.0f );
    static constexpr auto cBillboardTint = vec4( 1.0f );
    const Ref<Texture2D>& GetLightTexture( const LightType& lightType );
    void DrawBillboard( const Ref<Texture2D>& texture,
                        const Ref<Shader>& shader,
                        const vec3& position,
                        const vec2& size = vec2( 1.0f ),
                        const vec4& tintColor = vec4( 1.0f ) );
    void DrawEditorBillboards( Framebuffer& framebuffer, Scene& scene );


    // Draw entity id to framebuffer for object picking
    void DrawEntityIds( Framebuffer& framebuffer, Scene& scene );
    void DrawBillboardEntityId( const Entity entity, const vec3& position, const vec2& size = vec2( 1.0f ) );

public:
    Timer mTimer;
    Camera mActiveCamera;
    Renderer mRenderer;
    // Attached project to run
    Project& mProject;

    // Shaders for entity ID rendering
    Ref<Shader> mEntityIdShader;
    Ref<Shader> mEntityIdBillboardShader;

    // Visualization boxes and physics shapes
    Ref<Shader> mWhiteShader;

    struct MeshHelpers
    {
        VertexBufferData mVertices;
        vector<u32> mIndices;
        Mesh mMesh;
    };
    MeshHelpers mBoundingBoxes;
    MeshHelpers mPhysicsObjects;

    // Visualization Camera, Lights billboards textures
    Ref<Mesh> mBillboardQuad;
    Ref<Shader> mBillboardShader;
    Ref<Texture2D> mSceneCameraTexture;
    Ref<Texture2D> mScenePointLightTexture;
    Ref<Texture2D> mSceneSpotLightTexture;
    Ref<Texture2D> mSceneDirLightTexture;
};

}
