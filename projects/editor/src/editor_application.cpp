
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

      mEntityIdViewport( Framebuffer( Texture2DSpecification::CreateObjectId( VIEWPORT_SIZE ),
                                      Texture2DSpecification::CreateDepth( VIEWPORT_SIZE ) ) ),
      mEntityIdShader( LoadShader( ENTITY_PICKING_SHADER ) ),

      mProject( mWindow.GetWindowInput() ),
      mEngine( mProject ),
      
      mProjectResourcesHotReloader( mProject ),
      mEditorUserInterface( *this )
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

                mEngine.mActiveCamera = mSceneCamera;
                mEngine.DrawScene( mSceneViewport, mProject.mScene );
                mEngine.DrawBoundingBoxes( mSceneViewport, mProject.mScene );
                mEngine.DrawPhysicsShapes( mSceneViewport, mProject.mScene );
                DrawEntityIds();
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

void BubbleEditor::DrawEntityIds()
{
    // Draw scene's entity ids to buffer
    mEntityIdViewport.Bind();
    mEngine.mRenderer.ClearScreenUint( uvec4( 0 ) );
    mProject.mScene.ForEach<ModelComponent, TransformComponent>(
        [&]( Entity entity,
             ModelComponent& model,
             TransformComponent& transform )
    {
        mEntityIdShader->SetUni1ui( "uObjectId", (u32)entity );
        mEngine.mRenderer.DrawModel( model, transform.TransformMat(), mEntityIdShader );
    } );
}

}
