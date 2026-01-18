
#include "editor_application/editor_application.hpp"
#include "engine/utils/geometry.hpp"
#include <sol/sol.hpp>

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

      mEntityIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                      Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),
      mEntityIdBillboardShader( LoadShader( "./resources/shaders/object_picking_billboard" ) ),

      mProject( mWindow.GetWindowInput() ),
      mProjectResourcesHotReloader( mProject ),
      mEngine( mProject ),

      mEditorUserInterface( *this ),
       
      mEditorResources
      {
         .mSceneCameraTexture = LoadTexture2D( SCENE_CAMERA_TEXTURE ),
         .mScenePointLightTexture = LoadTexture2D( SCENE_POINT_LIGHT_TEXTURE ),
         .mSceneSpotLightTexture = LoadTexture2D( SCENE_SPOT_LIGHT_TEXTURE ),
         .mSceneDirLightTexture = LoadTexture2D( SCENE_DIR_LIGHT_TEXTURE )
      }
{
    mWindow.SetVSync( false );
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

                mEngine.mActiveCamera = (Camera)mSceneCamera;
                mEngine.DrawScene( mSceneViewport, mProject.mScene );
                DrawEditorBillboards( mSceneViewport, mProject.mScene );
                DrawEntityIds();
                break;
            }
            case EditorMode::Running:
            {
                try
                {
                    // temp
                    mSceneCamera.OnUpdate( deltaTime );
                    mEngine.mActiveCamera = (Camera)mSceneCamera;

                    mEngine.OnUpdate();
                    mEngine.DrawScene( mSceneViewport );
                }
                catch ( const std::exception& e )
                {
                    LogError( e.what() );
                    mEditorMode = EditorMode::Editing;
                    mProject.mScene = mSceneSave;
                };
                break;
            }
        }

        if ( mUIGlobals.mDrawBoundingBoxes )
            mEngine.DrawBoundingBoxes( mSceneViewport, mProject.mScene );
        if ( mUIGlobals.mDrawPhysicsShapes )
            mEngine.DrawPhysicsShapes( mSceneViewport, mProject.mScene );

        mWindow.ImGuiBegin();
        //ImGui::ShowDemoWindow();
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
    if ( mEditorMode == EditorMode::Editing and
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::F5 ) )
    {
        try
        {
            mEditorMode = EditorMode::Running;
            mSceneSave = mProject.mScene;
            mEngine.OnStart();
        }
        catch ( const std::exception& e )
        {
            LogError( e.what() );
            mEditorMode = EditorMode::Editing;
            mProject.mScene = mSceneSave;
        };
    }

    if ( mEditorMode == EditorMode::Running and
         mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::F6 ) )
    {
        mEditorMode = EditorMode::Editing;
        mProject.mScene = mSceneSave;
    }


    // Save project
    if ( mWindow.GetWindowInput().IsKeyClicked( KeyboardKey::S ) and
         mWindow.GetWindowInput().KeyMods().CONTROL and
         mEditorMode == EditorMode::Editing )
    {
        if ( mProject.IsValid() )
            mProject.Save();
    }

}


void BubbleEditor::DrawEntityIds()
{
    // Draw scene's entity ids to buffer
    mEntityIdViewport.Bind();
    mEngine.mRenderer.ClearScreenUint( uvec4( 0 ) );


    // Draw 3D models
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
        [&]( const Entity entity,
                  const ModelComponent& modelComponent,
                  const TransformComponent& transformComponent )
    {
        mEntityIdShader->SetUni1u( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawModel( modelComponent.mModel, mEntityIdShader, transformComponent.TransformMat() );
    } );


    // Draw camera billboards
    mProject.mScene.ForEach<CameraComponent, TransformComponent>(
        [&]( const Entity entity,
             const CameraComponent& cameraComponent,
             const TransformComponent& transformComponent )
    {
        mEntityIdBillboardShader->SetUni1u( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawBillboardEntityId(
            mEntityIdBillboardShader,
            transformComponent.mPosition,
            cBillboardSize
        );
    } );

    // Draw light billboards
    mProject.mScene.ForEach<LightComponent, TransformComponent>(
        [&]( const Entity entity,
             const LightComponent& lightComponent,
             const TransformComponent& transformComponent )
    {
        mEntityIdBillboardShader->SetUni1u( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawBillboardEntityId(
            mEntityIdBillboardShader,
            transformComponent.mPosition,
            cBillboardSize
        );
    } );
}


const Ref<Texture2D>& BubbleEditor::GetLightTexture( const LightType& lightType )
{
    switch ( lightType )
    {
        case LightType::Point:
            return mEditorResources.mScenePointLightTexture;
        case LightType::Spot:
            return mEditorResources.mSceneSpotLightTexture;
        case LightType::Directional:
            return mEditorResources.mSceneDirLightTexture;
    }
    BUBBLE_ASSERT( false, "Unknown light type" );
    return mEditorResources.mSceneCameraTexture;
}

void BubbleEditor::DrawEditorBillboards( Framebuffer& framebuffer, Scene& scene )
{
    framebuffer.Bind();

    // Camera icons (billboards)
    scene.ForEach<CameraComponent, TransformComponent>(
        [&]( const Entity entity,
             const CameraComponent& cameraComponent,
             const TransformComponent& transformComponent )
    {
        mEngine.mRenderer.DrawBillboard(
            mEditorResources.mSceneCameraTexture,
            mEngine.mBillboardShader,
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
        mEngine.mRenderer.DrawBillboard(
            lightTexture,
            mEngine.mBillboardShader,
            transformComponent.mPosition,
            cBillboardSize,
            cBillboardTint
        );
    } );
}

} // namespace bubble
