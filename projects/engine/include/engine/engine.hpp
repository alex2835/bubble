#pragma once
#include "engine/renderer/camera.hpp"
#include "engine/renderer/renderer.hpp"

namespace bubble
{
class Project;
class Scene;

struct Engine
{
    Timer mTimer;
    Camera mActiveCamera;
    Renderer mRenderer;
    
    // Attached project to run
    Project& mProject;

    // Visualization boxes and physics shapes
    Ref<Shader> mWhiteShader;
    Ref<Shader> mBillboardShader;

    struct MeshHelpers
    {
        VertexBufferData mVertices;
        vector<u32> mIndices;
        Mesh mMesh;
    };
    MeshHelpers mBoundingBoxes;
    MeshHelpers mPhysicsObjects;

    Engine( Project& project );
    void OnStart();
    void OnEnd();
    void OnUpdate();
    void DrawScene( Framebuffer& framebuffer );

    void DrawScene( Framebuffer& framebuffer, Scene& scene );
    void DrawBoundingBoxes( Framebuffer& framebuffer, Scene& scene );
    void DrawPhysicsShapes( Framebuffer& framebuffer, Scene& scene );
};

}
