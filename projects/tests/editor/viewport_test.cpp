// The viewport: what a click picks - an instance of a prefab whole, then its
// part -, the gizmo's matrix taken back apart, and on the GPU a project's
// cube picked by a click on the picture and moved by the gizmo, one step.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/viewport.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/world/transform.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>
#include <imgui.h>

// After ImGui's own header, which it takes as given.
#include <ImGuizmo.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}
}

TEST_CASE( "A click in the viewport picks an instance of a prefab whole, and its part the second time" )
{
    Scene scene;
    const Entity player = CreateChild( scene, scene.Root(), Make( "player" ) );
    const Entity body = CreateChild( scene, player, Make( "body" ) );
    const Entity crate = CreateChild( scene, scene.Root(), Make( "crate" ) );
    scene.Add<PrefabInstance>( player );
    scene.Add<PrefabLink>( player, PrefabLink{ .mInstance = player, .mNode = 0 } );
    scene.Add<PrefabLink>( body, PrefabLink{ .mInstance = player, .mNode = 1 } );
    Selection selection( scene );

    CHECK( PickTarget( scene, selection, body ) == player );
    selection.Select( player );
    CHECK( PickTarget( scene, selection, body ) == body );
    selection.Select( body );
    CHECK( PickTarget( scene, selection, body ) == body );
    selection.Select( crate );
    CHECK( PickTarget( scene, selection, body ) == player );
    CHECK( PickTarget( scene, selection, crate ) == crate );
}

TEST_CASE( "The gizmo's matrix comes apart into the position, turn and scale it was made of" )
{
    const Transform made{ .mPosition = vec3( 1, 2, 3 ),
                          .mRotation = vec3( 0.3f, -0.8f, 0.2f ),
                          .mScale = vec3( 2, 1, 3 ) };
    const mat4 matrix = glm::translate( mat4( 1.0f ), made.mPosition ) * glm::mat4_cast( RotationOf( made ) ) *
                        glm::scale( mat4( 1.0f ), made.mScale );
    const Decomposed apart = Decompose( matrix );
    CHECK( glm::length( apart.mPosition - made.mPosition ) < 1e-5f );
    CHECK( glm::length( apart.mScale - made.mScale ) < 1e-5f );
    CHECK( glm::length( AnglesOf( apart.mRotation ) - made.mRotation ) < 1e-4f );

    // A mirror is a negative scale.
    CHECK( Decompose( glm::scale( mat4( 1.0f ), vec3( -1, 1, 1 ) ) ).mScale.x == doctest::Approx( -1.0f ) );
}

#if defined( BUBBLE_IMPORT_MODELS ) and not defined( __EMSCRIPTEN__ )
namespace
{
constexpr string_view cProject =
    R"({ "version": 1, "name": "view", "start_level": { "path": "main.scene" }, "actions": {} })";

// A unit cube, its faces turned outward.
constexpr string_view cCube = R"(v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
f 1 4 3 2
f 5 6 7 8
f 1 5 8 4
f 2 3 7 6
f 1 2 6 5
f 4 8 7 3
)";

constexpr string_view cCubeImport = R"({ "version": 1, "asset": "c0be0000c0be0000c0be0000c0be0000", "settings": {} })";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "cube",
        "components": {
          "transform": {},
          "model": { "asset": { "asset": "c0be0000c0be0000c0be0000c0be0000", "path": "cube.obj" } }
        }
      }
    ]
  }
})";

constexpr ImVec2 cSize{ 320, 240 };

struct View
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "cube.obj", string( cCube ) },
                                 { "cube.obj.import", string( cCubeImport ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    Scope<Gpu> mGpu = StartGpu();
    Scope<Renderer> mRenderer;
    ImGuiContextScope mContext;
    Scope<Viewport> mViewport;

    View()
    {
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
        REQUIRE( mProject->TryOpenStartLevel() );
        mRenderer = CreateScope<Renderer>( *mGpu, mProject->GetAssets() );
        mViewport = CreateScope<Viewport>( *mProject, *mGpu, *mRenderer );
        Editor::SetUpImGui( 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = cSize;
    }

    ~View()
    {
        // The pictures go before the Gpu does.
        mViewport.reset();
        mRenderer.reset();
    }

    Document& GetDocument() { return *mProject->GetWorkspace().TryActive(); }
    const Scene& GetScene() { return GetDocument().GetScene(); }
    Entity Cube() { return *TryResolve( GetScene(), EntityPath{ Make( "/cube" ) } ); }

    void Frame()
    {
        mProject->GetApp().GetJobs().RunMainQueue();
        mGpu->Poll();
        DocumentWorld& shown = ViewOf( mProject->GetApp(), GetDocument() );
        World& world = shown.Get();
        mViewport->Prepare( GetDocument(), world );
        world.Frame( 1.0 / 60 );
        shown.PlaceCamera( world.Get<RenderScene>() );
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( cSize );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        ImGui::Begin( "Viewport", nullptr, ImGuiWindowFlags_NoDecoration );
        ImGui::PopStyleVar();
        mViewport->Show( GetDocument(), shown.Camera() );
        ImGui::End();
        ImGui::Render();
        mViewport->Draw( world, shown.Camera() );
    }

    void MoveTo( ImVec2 at )
    {
        ImGui::GetIO().AddMousePosEvent( at.x, at.y );
        Frame();
    }

    void Button( bool down )
    {
        ImGui::GetIO().AddMouseButtonEvent( 0, down );
        Frame();
    }

    // Frames until `done` says so, or a hundred.
    bool Until( const function<bool()>& done )
    {
        for ( int frame = 0; frame < 100; ++frame )
        {
            if ( done() )
                return true;
            Frame();
        }
        return done();
    }
};
}

TEST_CASE( "A click on the viewport's picture picks what is drawn there; the gizmo moves it, one step" )
{
    View view;
    // The cube comes once its model has loaded.
    REQUIRE( view.Until( [&] {
        const World& world = WorldOf( view.mProject->GetApp(), view.GetDocument() );
        const auto& items = const_cast<World&>( world ).Get<RenderScene>().mItems;
        return not items.empty() and items.front().mModel.TryGet().HasValue();
    } ) );

    // The editor's camera looks at the origin: the cube is in the middle.
    const ImVec2 middle( cSize.x * 0.5f, cSize.y * 0.5f );
    view.MoveTo( middle );
    view.Button( true );
    view.Button( false );
    REQUIRE( view.Until( [&] { return view.GetDocument().GetSelection().Active() == view.Cube(); } ) );

    // Selected, it shows its box.
    view.Frame();
    CHECK( not WorldOf( view.mProject->GetApp(), view.GetDocument() ).Get<RenderScene>().mLines.empty() );

    // The gizmo's middle square drags it across the view.
    view.MoveTo( middle );
    view.Button( true );
    for ( int step = 1; step <= 6; ++step )
        view.MoveTo( ImVec2( middle.x + 8.0f * static_cast<f32>( step ), middle.y ) );
    view.Button( false );
    view.Frame();
    const vec3 moved = view.GetScene().Get<Transform>( view.Cube() ).mPosition;
    CHECK( glm::length( moved ) > 0.1f );
    REQUIRE( view.mProject->Run( "edit.undo" ) );
    CHECK( glm::length( view.GetScene().Get<Transform>( view.Cube() ).mPosition ) < 1e-5f );
    CHECK( not view.GetDocument().IsDirty() );
}
#endif
