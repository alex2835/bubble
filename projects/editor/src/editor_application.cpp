
#include "editor_application/editor_application.hpp"
#include "engine/utils/geometry.hpp"
#include <sol/sol.hpp>
#include <print>

namespace bubble
{
constexpr uvec2 WINDOW_SIZE{ 1920, 1080 };
constexpr uvec2 VIEWPORT_SIZE{ 800, 640 };

BubbleEditor::BubbleEditor()
    : mWindow( Window( "Bubble", WINDOW_SIZE ) ),
    mEditorMode( EditorMode::Editing ),
    mSceneCamera( SceneCamera( mWindow.GetWindowInput(), vec3( 0, 0, 100 ) ) ),

    mSceneViewport( Framebuffer( Texture2DSpecification::CreateRGBA8( VIEWPORT_SIZE ),
                                 Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
    mObjectIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                    Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
    mObjectIdShader( LoadShader( OBJECT_PICKING_SHADER ) ),

    mProject( mWindow.GetWindowInput() ),
    mEngine( mProject ),

    mWhiteShader( LoadShader( WHITE_SHADER ) ),
    mBBoxsMesh( Mesh( "AABB", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) ),
    mPhysicsObjectsMesh( Mesh( "Physics", BasicMaterial(), VertexBufferData{}, vector<u32>{}, BufferType::Dynamic ) ),

    mProjectResourcesHotReloader( mProject ),
    mEditorUserInterface( *this )
{
}


void BubbleEditor::Run()
{
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mWindow.ShouldClose() )
#endif
    {
        mWindow.PollEvents();
        OnUpdate();
        mTimer.OnUpdate();
        auto deltaTime = mTimer.GetDeltaTime();

        switch ( mEditorMode )
        {
            case EditorMode::Editing:
            {
                mSceneCamera.OnUpdate( deltaTime );
                mProjectResourcesHotReloader.OnUpdate();
                mEditorUserInterface.OnUpdate( deltaTime );
                DrawProjectScene();

                DrawBBoxes();
                DrawPhysics();
                break;
            }
            case EditorMode::Running:
            {
                // temp
                mSceneCamera.OnUpdate( deltaTime );
                mEngine.mActiveCamera = mSceneCamera;

                mEngine.OnUpdate();
                mEngine.DrawScene( mSceneViewport );
                break;
            }
        }
        mWindow.ImGuiBegin();
        mEditorUserInterface.OnDraw( deltaTime );
        mWindow.ImGuiEnd();
        mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}

void BubbleEditor::OpenProject( const path& projectPath )
{
    LogInfo( "Open project {}", projectPath.string() );
    mUIGlobals.mNeedUpdateProjectWindow = true;
    mProject.Open( projectPath );
}

void BubbleEditor::OnUpdate()
{
    /// Switch Editing/GameRunning modes
    if ( mWindow.GetWindowInput().IsKeyCliked( KeyboardKey::F5 )
         and mEditorMode == EditorMode::Editing )
    {
        mEditorMode = EditorMode::Running;
        mSceneSave = mProject.mScene;
        mEngine.OnStart();
    }

    if ( mWindow.GetWindowInput().IsKeyCliked( KeyboardKey::F6 )
         and mEditorMode == EditorMode::Running )
    {
        mProject.mScene = mSceneSave;
        mEditorMode = EditorMode::Editing;
    }


    /// Copy entity
    if ( mWindow.GetWindowInput().IsKeyCliked( KeyboardKey::V ) and
         mWindow.GetWindowInput().KeyMods().CONTROL and
         mEditorMode == EditorMode::Editing and
         mSelectedEntity )
    {
        auto newEntity = mProject.mScene.CopyEntity( mSelectedEntity );
        auto& tag = mProject.mScene.GetComponent<TagComponent>( newEntity );
        tag.mName = tag.mName + " Copy";

        auto& trans = mProject.mScene.GetComponent<TransformComponent>( newEntity );
        trans.mPosition += vec3( 1 );

        mSelectedEntity = newEntity;
    }

}

void BubbleEditor::DrawProjectScene()
{
    // Draw scene's objectId 
    mObjectIdViewport.Bind();
    mEngine.mRenderer.ClearScreenUint( uvec4( 0 ) );
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity entity,
             ModelComponent& model,
             TransformComponent& transform )
    {
        mObjectIdShader->SetUni1ui( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawModel( model, transform.TransformMat(), mObjectIdShader );
    } );


    // Draw scene
    mSceneViewport.Bind();
    mEngine.mRenderer.ClearScreen( vec4( 0.2f, 0.3f, 0.3f, 1.0f ) );
    mEngine.mRenderer.SetUniformBuffers( mSceneCamera, mSceneViewport );
    mProject.mScene.ForEach<ModelComponent, ShaderComponent, TransformComponent>(
        [&]( Entity _,
             ModelComponent& model,
             ShaderComponent& shader,
             TransformComponent& transform )
    {
        // Draw model
        mEngine.mRenderer.DrawModel( model, transform.TransformMat(), shader );
    } );
}


void BubbleEditor::DrawBBoxes()
{
    if ( mProject.mScene.Size() == 0 )
        return;

    mBBoxesVerts.Clear();
    mBBoxesIndices.clear();

    u32 elementIndexStride = 0;
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity _,
             ModelComponent& model,
             TransformComponent& transform )
    {
        const mat4 trans = transform.TransformMat();
        AABB box = CalculateTransformedBBox( model->mBBox, trans );
        vec3 min = box.getMin();
        vec3 max = box.getMax();

        mBBoxesVerts.mPositions.push_back( vec3( min.x, min.y, min.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( max.x, min.y, min.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( max.x, max.y, min.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( min.x, max.y, min.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( min.x, min.y, max.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( max.x, min.y, max.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( max.x, max.y, max.z ) );
        mBBoxesVerts.mPositions.push_back( vec3( min.x, max.y, max.z ) );

        array<u32, 24> indices = {
            0, 1,  1, 2,  2, 3,  3, 0,// bottom lines
            4, 5,  5, 6,  6, 7,  7, 4,// top lines
            0, 4,  1, 5,  2, 6,  3, 7 // vertical edges lines
        };
        mBBoxesIndices.append_range( indices | std::views::transform( [&]( u32 idx ) { return idx + elementIndexStride; } ) );
        elementIndexStride = (u32)mBBoxesVerts.mPositions.size();
    } );
    mBBoxsMesh.UpdateDynamicVertexBufferData( mBBoxesVerts, mBBoxesIndices );
    mEngine.mRenderer.DrawMesh( mBBoxsMesh, mWhiteShader, mat4( 1 ), DrawingPrimitive::Lines );
}


void BubbleEditor::DrawPhysics()
{
    if ( mProject.mScene.Size() == 0 )
        return;

    mPhysicsObjectsVerts.Clear();
    mPhysicsObjectsIndices.clear();

    u32 elementIndexStride = 0;
    mProject.mScene.ForEach<PhysicsComponent, TransformComponent>(
        [&]( Entity _,
             PhysicsComponent& physics,
             TransformComponent& transform )
    {
        const mat4 trans = transform.TranslationRotationMat();

        auto body = physics.mPhysicsObject.getBody();
        auto shape = physics.mPhysicsObject.getShape();
        switch ( shape->getShapeType() )
        {
            case SPHERE_SHAPE_PROXYTYPE:
            {
                auto sphereShape = static_cast<const btSphereShape*>( shape );
                auto [vertices, indices] = GenerateSphereLinesShape( sphereShape->getRadius() );
                mPhysicsObjectsVerts.mPositions.append_range( vertices | std::views::transform( [&]( vec3 vert ) { return vec3( trans * vec4( vert, 1 ) ); } ) );
                mPhysicsObjectsIndices.append_range( indices | std::views::transform( [&]( u32 idx ) { return idx + elementIndexStride; } ) );
            } break;
            case BOX_SHAPE_PROXYTYPE:
            {
                auto boxShape = static_cast<const btBoxShape*>( shape );
                auto he = boxShape->getHalfExtentsWithMargin();
                auto [vertices, indices] = GenerateCubeLinesShape( vec3( he.x(), he.y(), he.z() ) );
                mPhysicsObjectsVerts.mPositions.append_range( vertices | std::views::transform( [&]( vec3 vert ) { return vec3( trans * vec4( vert, 1 ) ); } ) );
                mPhysicsObjectsIndices.append_range( indices | std::views::transform( [&]( u32 idx ) { return idx + elementIndexStride; } ) );
            } break;
        }
        elementIndexStride = (u32)mPhysicsObjectsVerts.mPositions.size();
    } );
    mPhysicsObjectsMesh.UpdateDynamicVertexBufferData( mPhysicsObjectsVerts, mPhysicsObjectsIndices );
    mEngine.mRenderer.DrawMesh( mPhysicsObjectsMesh, mWhiteShader, mat4( 1 ), DrawingPrimitive::Lines );
}

}
