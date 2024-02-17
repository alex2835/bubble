
#include "editor_app.hpp"
#include "engine/utils/emscripten_main_loop.hpp"
#include <functional>

namespace bubble
{
constexpr std::string_view vert_shader = R"shader(
    #version 330 core
    layout(location = 0) in vec3 aPosition;
    
    layout(std140) uniform VertexUniformBuffer
    {
        mat4 uProjection;
        mat4 uView;
    };
    uniform mat4 uModel;

    void main()
    {
        gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    }
)shader";

constexpr std::string_view frag_shader = R"shader(
    #version 330 core
    out vec4 FragColor;
    void main()
    {
        FragColor = vec4(1. );
    }
)shader";


constexpr std::string_view cPickingVertShader = R"shader(
    #version 330 core
    layout(location = 0) in vec3 aPosition;
    
    layout(std140) uniform VertexUniformBuffer
    {
        mat4 uProjection;
        mat4 uView;
    };
    uniform mat4 uModel;

    void main()
    {
        gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
    }
)shader";

constexpr std::string_view cPickingFragShader = R"shader(
    #version 330 core
    layout(location = 0)out uint objectId;

    uniform uint uObjectId;
    void main()
    {
        objectId = uObjectId;
    }
)shader";


constexpr WindowSize cWindowSize{ 1200, 720 };
constexpr uvec2 cViewportSize{ 800, 640 };

BubbleEditor::BubbleEditor()
    : mState{ Window( "Bubble", cWindowSize ),
              Framebuffer( Texture2DSpecification::CreateRGBA8( cViewportSize ),
                           Texture2DSpecification::CreateDepth( cViewportSize ) ),
              Framebuffer( Texture2DSpecification::CreateObjectId( cViewportSize ),
                           //Texture2DSpecification::CreateRGBA32( cViewportSize ),
                           Texture2DSpecification::CreateDepth( cViewportSize ) ),
              SceneCamera( vec3( 0, 15, 70 ) ) },
      mInterfaceLoader( mState, mEngine, mState.mWindow.GetImGuiContext() )
{
    ImGui::SetCurrentContext( mState.mWindow.GetImGuiContext() );

    // Components draw ability
    ComponentsOnDrawStorage::Add<TagComponent>();
    ComponentsOnDrawStorage::Add<TransformComponent>();
    ComponentsOnDrawStorage::Add<ModelComponent>();

    // For object selecting

    // Editor's viewport interface
    mInterfaceLoader.LoadInterfaces();
}


void BubbleEditor::Run()
{
    path model_path = R"(C:\Users\sa007\Desktop\projects\Bubble0.5\test_project\resources\models\crysis\nanosuit.obj)";
    auto model = mEngine.mLoader.LoadModel( model_path );
    auto shader = mEngine.mLoader.LoadShader( "test_shader", vert_shader, frag_shader );
    model->mShader = shader;

    Entity entity = mEngine.mScene.CreateEntity();
    entity.AddComponet<TagComponent>( "test" )
          .AddComponet<ModelComponent>( model )
          .AddComponet<TransformComponent>();
    mState.mSelectedEntity = entity;


    auto pickingShader = mEngine.mLoader.LoadShader( "picking_shader", cPickingVertShader, cPickingFragShader );


#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ( !mState.mWindow.ShouldClose() )
#endif
    {
        // Events
        const auto& events = mState.mWindow.PollEvents();
        for ( const auto& event : events )
            mState.mSceneCamera.OnEvent( event );

        // Update
        mEngine.OnUpdate();
        auto dt = mEngine.mTimer.GetDeltaTime();
        mState.mSceneCamera.OnUpdate( dt );
        mInterfaceLoader.OnUpdate( dt );

        // Draw 
        mEngine.DrawScene( mState.mSceneCamera, mState.mSceneViewport );
        mEngine.DrawSceneObjectId( mState.mSceneCamera, mState.mObjectIdViewport, pickingShader );

        Framebuffer::BindWindow( mState.mWindow );
        mState.mWindow.ImGuiBegin();
        //ImGui::ShowDemoWindow();
        mInterfaceLoader.OnDraw( dt );
        mState.mWindow.ImGuiEnd();

        mState.mWindow.OnUpdate();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}


}
