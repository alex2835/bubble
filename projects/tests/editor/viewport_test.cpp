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
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"
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

TEST_CASE( "The floor's grid is around the camera's focus, its step a power of ten by the camera's distance" )
{
    EditorCamera camera;
    camera.mFocus = vec3( 3.4f, 2.0f, -7.6f );
    camera.mDistance = 12.0f;
    const vector<DebugLine> lines = FloorGrid( camera );
    REQUIRE_FALSE( lines.empty() );
    for ( const DebugLine& line : lines )
    {
        CHECK( ( line.mFrom.y == 0.0f and line.mTo.y == 0.0f ) );
        CHECK( ( line.mColor.mA > 0.0f and line.mColor.mA <= 1.0f ) );
    }
    // Where the lines along Z stand, a step apart, around x = 3.
    const auto standing = [&]( const vector<DebugLine>& grid ) {
        vector<f32> xs;
        for ( const DebugLine& line : grid )
            if ( line.mFrom.x == line.mTo.x and not ranges::contains( xs, line.mFrom.x ) )
                xs.push_back( line.mFrom.x );
        ranges::sort( xs );
        return xs;
    };
    const vector<f32> xs = standing( lines );
    REQUIRE( xs.size() > 2 );
    CHECK( xs[1] - xs[0] == doctest::Approx( 1.0f ) );
    CHECK( ( xs.front() < 3.0f and xs.back() > 3.0f ) );
    // The Z axis at x = 0 is blue, whole by the focus and faded far off.
    const auto onAxis = [&]( f32 z ) {
        return *ranges::find_if( lines, [&]( const DebugLine& line ) {
            return line.mFrom.x == 0.0f and line.mTo.x == 0.0f and line.mFrom.z <= z and line.mTo.z >= z;
        } );
    };
    CHECK( onAxis( -7.0f ).mColor.mB > onAxis( -7.0f ).mColor.mR );
    CHECK( onAxis( -7.0f ).mColor.mA == doctest::Approx( 1.0f ) );
    CHECK( onAxis( 30.0f ).mColor.mA < 0.5f );
    // Along X at z = -10, a tenth line, brighter than its neighbour at -9.
    const auto along = [&]( f32 z ) {
        return *ranges::find_if( lines, [&]( const DebugLine& line ) {
            return line.mFrom.z == doctest::Approx( z ) and line.mTo.z == doctest::Approx( z ) and
                   line.mFrom.x <= 3.0f and line.mTo.x >= 3.0f;
        } );
    };
    CHECK( along( -10.0f ).mColor.mR > along( -9.0f ).mColor.mR );
    CHECK( along( 0.0f ).mColor.mR > along( 0.0f ).mColor.mB );

    camera.mDistance = 1.8f;
    const vector<f32> close = standing( FloorGrid( camera ) );
    CHECK( close[1] - close[0] == doctest::Approx( 0.1f ) );
    camera.mDistance = 120.0f;
    const vector<f32> far = standing( FloorGrid( camera ) );
    CHECK( far[1] - far[0] == doctest::Approx( 10.0f ) );
}

TEST_CASE( "The rest selected go with the active one the gizmo drags: moved, turned about it, scaled as it is" )
{
    Scene scene;
    const Entity a = CreateChild( scene, scene.Root(), Make( "a" ) );
    const Entity b = CreateChild( scene, scene.Root(), Make( "b" ) );
    const Entity c = CreateChild( scene, a, Make( "c" ) );
    for ( const Entity entity : { a, b, c } )
        scene.Add<Transform>( entity, Transform{} );
    scene.Get<Transform>( b ).mPosition = vec3( 2, 0, 0 );
    const vector<Entity> selected{ a, b, c };
    const auto near = []( vec3 got, vec3 wanted ) { return glm::length( got - wanted ) < 1e-4f; };

    const auto moved = MovedTogether( scene, selected, a, mat4( 1.0f ), glm::translate( mat4( 1.0f ), vec3( 1, 0, 0 ) ),
                                      GizmoTool::Move );
    // c is under a, and goes with it.
    REQUIRE( moved.size() == 2 );
    CHECK( moved[0].first == a );
    CHECK( near( moved[0].second.mPosition, vec3( 1, 0, 0 ) ) );
    CHECK( near( moved[1].second.mPosition, vec3( 3, 0, 0 ) ) );

    const auto turned =
        MovedTogether( scene, selected, a, mat4( 1.0f ),
                       glm::rotate( mat4( 1.0f ), glm::radians( 90.0f ), vec3( 0, 1, 0 ) ), GizmoTool::Turn );
    CHECK( near( turned[1].second.mPosition, vec3( 0, 0, -2 ) ) );

    const auto scaled =
        MovedTogether( scene, selected, a, mat4( 1.0f ), glm::scale( mat4( 1.0f ), vec3( 2.0f ) ), GizmoTool::Scale );
    CHECK( near( scaled[1].second.mScale, vec3( 2.0f ) ) );
    CHECK( near( scaled[1].second.mPosition, vec3( 2, 0, 0 ) ) );
}

TEST_CASE( "A turn the gizmo made is written in the angles nearest the ones it had" )
{
    const auto turnOf = []( vec3 angles ) { return RotationOf( Transform{ .mRotation = angles } ); };
    const auto near = []( vec3 got, vec3 wanted ) { return glm::length( got - wanted ) < 1e-3f; };
    // Past half a turn about Y it goes on, rather than jump to the other
    // side.
    CHECK( near( AnglesNear( turnOf( vec3( 0, 3.2f, 0 ) ), vec3( 0, 3.0f, 0 ) ), vec3( 0, 3.2f, 0 ) ) );
    // Tipped over past straight up about X: the same, not a flip of Y and Z.
    CHECK( near( AnglesNear( turnOf( vec3( 2.5f, 0, 0 ) ), vec3( 2.4f, 0, 0 ) ), vec3( 2.5f, 0, 0 ) ) );
    CHECK( near( AnglesNear( turnOf( vec3( 0.1f, 0.2f, 0.3f ) ), vec3( 0 ) ), vec3( 0.1f, 0.2f, 0.3f ) ) );
    // Whichever it picks, it is the same turn.
    const quat turn = turnOf( vec3( 2.0f, -1.0f, 0.5f ) );
    CHECK( abs( glm::dot( turnOf( AnglesNear( turn, vec3( 6.0f, 0, 0 ) ) ), turn ) ) == doctest::Approx( 1.0f ) );
}

TEST_CASE( "A rectangle over the picture takes what it covers there" )
{
    // Seen straight: the world's x and y from -1 to 1 across a picture of
    // 100 by 100, y up.
    Scene scene;
    const Entity middle = CreateChild( scene, scene.Root(), Make( "middle" ) );
    const Entity corner = CreateChild( scene, scene.Root(), Make( "corner" ) );
    const vector<pair<Entity, pair<vec3, vec3>>> boxes{
        { middle, { vec3( -0.1f ), vec3( 0.1f ) } }, { corner, { vec3( 0.5f, 0.5f, 0.0f ), vec3( 0.6f, 0.6f, 0.0f ) } }
    };
    const mat4 straight( 1.0f );
    CHECK( InRectangle( boxes, straight, vec2( 100 ), vec2( 40 ), vec2( 60 ) ) == vector<Entity>{ middle } );
    // Dragged the other way round, the top right corner: the second.
    CHECK( InRectangle( boxes, straight, vec2( 100 ), vec2( 90, 10 ), vec2( 70, 30 ) ) == vector<Entity>{ corner } );
    CHECK( InRectangle( boxes, straight, vec2( 100 ), vec2( 0 ), vec2( 100 ) ).size() == 2 );
    CHECK( InRectangle( boxes, straight, vec2( 100 ), vec2( 0, 90 ), vec2( 10, 100 ) ).empty() );
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
    // Where its window is, and whether it has a title bar over the picture -
    // as a docked one has its tab.
    ImVec2 mWindowAt{ 0, 0 };
    bool mTitled = false;

    // `level` instead of the cube's alone.
    explicit View( string_view level = cLevel )
    {
        mFiles["main.scene"] = string( level );
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
        Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( cSize.x + 200, cSize.y + 200 );
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
        // Its UI laid out for the picture, as the editor lays it.
        world.Get<UiWorld>().SetScreen( ivec2( static_cast<i32>( cSize.x ), static_cast<i32>( cSize.y ) ) );
        world.Frame( 1.0 / 60 );
        shown.Gathered();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        ImGui::SetNextWindowPos( mWindowAt );
        // A title bar on top of the picture, which stays its size.
        ImGui::SetNextWindowSize( mTitled ? ImVec2( cSize.x, cSize.y + ImGui::GetFrameHeight() ) : cSize );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        ImGui::Begin( "Viewport", nullptr,
                      mTitled ? ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                              : ImGuiWindowFlags_NoDecoration );
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

TEST_CASE( "A click on the scene's UI over the picture picks the element there" )
{
    View view( R"({
  "version": 1,
  "root": { "children": [ { "name": "hud", "components": { "ui_canvas": {} }, "children": [
    { "name": "panel", "components": {
      "ui_rect": { "position": "absolute", "left": "20", "top": "120", "width": "80", "height": "60" },
      "ui_image": { "color": [ 1, 0, 0, 1 ] } } } ] } ] }
})" );
    for ( int frame = 0; frame < 5; ++frame )
        view.Frame();
    // Laid out in the canvas's 720 pixels high, shown in the picture's 240:
    // a third of its size, from (20, 120) to (100, 180) there.
    view.MoveTo( ImVec2( 20, 50 ) );
    view.Button( true );
    view.Button( false );
    view.Frame();
    const Entity panel = *TryResolve( view.GetScene(), EntityPath{ Make( "/hud/panel" ) } );
    CHECK( view.GetDocument().GetSelection().Active() == panel );

    // Selected, it is dragged by its left and top, in the canvas's pixels,
    // one step.
    view.MoveTo( ImVec2( 20, 50 ) );
    view.Button( true );
    for ( int step = 1; step <= 5; ++step )
        view.MoveTo( ImVec2( 20 + 6.0f * static_cast<f32>( step ), 50 + 2.0f * static_cast<f32>( step ) ) );
    view.Button( false );
    view.Frame();
    const UiRect& rect = view.GetScene().Get<UiRect>( panel );
    CHECK( rect.mLeft == "110" );
    CHECK( rect.mTop == "150" );
    REQUIRE( view.mProject->Run( "edit.undo" ) );
    CHECK( view.GetScene().Get<UiRect>( panel ).mLeft == "20" );
}

TEST_CASE( "The right button flies the viewport's camera: the mouse turns it where it stands, the keys move it" )
{
    View view;
    view.Frame();
    const EditorCamera& camera = ViewOf( view.mProject->GetApp(), view.GetDocument() ).Camera();
    view.MoveTo( ImVec2( cSize.x * 0.5f, cSize.y * 0.5f ) );
    ImGui::GetIO().AddMouseButtonEvent( ImGuiMouseButton_Right, true );
    view.Frame();
    CHECK( view.mViewport->Steers() );

    // The platform's move turns it, ImGui's mouse standing still - the cursor
    // held -, where it stands.
    const vec3 eye = camera.Position();
    const f32 yaw = camera.mYaw;
    view.mViewport->MouseMoved( vec2( 100.0f, 0.0f ) );
    view.Frame();
    CHECK( camera.mYaw == doctest::Approx( yaw - 0.5f ) );
    CHECK( glm::length( camera.Position() - eye ) < 1e-4f );

    // W flies ahead, E up: the camera's keys, not the gizmo's.
    ImGui::GetIO().AddKeyEvent( ImGuiKey_W, true );
    for ( int frame = 0; frame < 6; ++frame )
        view.Frame();
    ImGui::GetIO().AddKeyEvent( ImGuiKey_W, false );
    view.Frame();
    CHECK( glm::dot( camera.Position() - eye, camera.Forward() ) == doctest::Approx( 0.5f ).epsilon( 0.05 ) );
    const vec3 ahead = camera.Position();
    ImGui::GetIO().AddKeyEvent( ImGuiKey_E, true );
    view.Frame();
    ImGui::GetIO().AddKeyEvent( ImGuiKey_E, false );
    view.Frame();
    CHECK( camera.Position().y - ahead.y == doctest::Approx( 5.0f / 60 ) );
    CHECK( view.mViewport->Tool() == GizmoTool::Move );

    // The wheel sets the speed while it flies.
    const f32 speed = camera.mFlySpeed;
    const f32 distance = camera.mDistance;
    ImGui::GetIO().AddMouseWheelEvent( 0.0f, 1.0f );
    view.Frame();
    CHECK( camera.mFlySpeed > speed );
    CHECK( camera.mDistance == distance );

    ImGui::GetIO().AddMouseButtonEvent( ImGuiMouseButton_Right, false );
    view.Frame();
    CHECK( not view.mViewport->Steers() );
}

TEST_CASE( "Alt and the left button go round the selection in the viewport, picking nothing" )
{
    View view;
    REQUIRE( view.Until( [&] {
        const World& world = WorldOf( view.mProject->GetApp(), view.GetDocument() );
        const auto& items = const_cast<World&>( world ).Get<RenderScene>().mItems;
        return not items.empty() and items.front().mModel.TryGet().HasValue();
    } ) );
    view.MoveTo( ImVec2( cSize.x * 0.5f, cSize.y * 0.5f ) );
    view.Button( true );
    view.Button( false );
    REQUIRE( view.Until( [&] { return view.GetDocument().GetSelection().Active() == view.Cube(); } ) );
    view.Frame();

    // The cube's middle is the origin: the camera goes round it, as far.
    const EditorCamera& camera = ViewOf( view.mProject->GetApp(), view.GetDocument() ).Camera();
    const f32 reach = glm::length( camera.Position() );
    const f32 yaw = camera.mYaw;
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Alt, true );
    view.MoveTo( ImVec2( cSize.x - 20, cSize.y - 20 ) );
    view.Button( true );
    CHECK( view.mViewport->Steers() );
    view.mViewport->MouseMoved( vec2( 60.0f, 20.0f ) );
    view.Frame();
    view.Button( false );
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Alt, false );
    for ( int frame = 0; frame < 5; ++frame )
        view.Frame();
    CHECK( not view.mViewport->Steers() );
    CHECK( camera.mYaw == doctest::Approx( yaw - 0.3f ) );
    CHECK( glm::length( camera.Position() ) == doctest::Approx( reach ).epsilon( 1e-3 ) );
    // The press was the camera's: the cube is still the one selected.
    CHECK( view.GetDocument().GetSelection().Active() == view.Cube() );
}

TEST_CASE( "A rectangle drawn over the viewport's picture selects what is in it" )
{
    View view;
    REQUIRE( view.Until( [&] {
        const World& world = WorldOf( view.mProject->GetApp(), view.GetDocument() );
        const auto& items = const_cast<World&>( world ).Get<RenderScene>().mItems;
        return not items.empty() and items.front().mModel.TryGet().HasValue();
    } ) );
    view.Frame();
    // From the bottom right, clear of the tools in the top left corner.
    const ImVec2 from( cSize.x - 6, cSize.y - 6 );
    const ImVec2 to( 10, 40 );
    view.MoveTo( from );
    view.Button( true );
    for ( int step = 1; step <= 4; ++step )
    {
        const f32 share = static_cast<f32>( step ) / 4.0f;
        view.MoveTo( ImVec2( from.x + ( to.x - from.x ) * share, from.y + ( to.y - from.y ) * share ) );
    }
    view.Button( false );
    view.Frame();
    CHECK( view.GetDocument().GetSelection().Active() == view.Cube() );
}
#endif

TEST_CASE(
    "The gizmo answers the mouse where it is drawn, in a window with a title bar over the picture as in one without" )
{
    // Where the mouse is over the gizmo, a row at a time across its middle
    // and along its X arrow, from the picture's middle.
    const auto over = []( bool titled ) {
        View view;
        view.mWindowAt = titled ? ImVec2( 60, 40 ) : ImVec2( 0, 0 );
        view.mTitled = titled;
        REQUIRE( view.Until( [&] {
            const World& world = WorldOf( view.mProject->GetApp(), view.GetDocument() );
            const auto& items = const_cast<World&>( world ).Get<RenderScene>().mItems;
            return not items.empty() and items.front().mModel.TryGet().HasValue();
        } ) );
        const f32 title = titled ? ImGui::GetFrameHeight() : 0.0f;
        const ImVec2 middle( view.mWindowAt.x + cSize.x * 0.5f, view.mWindowAt.y + title + cSize.y * 0.5f );
        view.MoveTo( middle );
        view.Button( true );
        view.Button( false );
        REQUIRE( view.Until( [&] { return view.GetDocument().GetSelection().Active() == view.Cube(); } ) );
        vector<pair<f32, f32>> rows;
        for ( const f32 dx : { 0.0f, 15.0f, 25.0f } )
            for ( f32 dy = -40; dy <= 40; dy += 2 )
            {
                view.MoveTo( ImVec2( middle.x + dx, middle.y + dy ) );
                view.Frame();
                if ( ImGuizmo::IsOver() )
                    rows.emplace_back( dx, dy );
            }
        return rows;
    };
    const vector<pair<f32, f32>> plain = over( false );
    CHECK( not plain.empty() );
    CHECK( over( true ) == plain );
}
