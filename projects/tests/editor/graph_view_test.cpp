// An animation graph's canvas and the inspector beside it, in ImGui with no
// window and no GPU: nodes where the document keeps them, a node dragged as
// one step, a state picked on the canvas edited in the inspector, Delete
// taking it out - the mouse and the keys as ImGui's events.
#include "../animation/skinned_model.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/graph_view.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/editor/play_session.hpp"
#include <doctest.h>
#include <imgui.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({ "version": 1, "name": "graphs", "actions": {} })";

constexpr string_view cGraph = R"({
  "parameters": { "grounded": true },
  "entry": "idle",
  "states": { "idle": {}, "fall": {}, "land": {} },
  "transitions": [
    { "from": "idle", "to": "fall", "when": "!grounded" },
    { "from": "fall", "to": "land", "when": "grounded" },
    { "from": "land", "to": "return", "exit_time": 0.9 }
  ],
  "editor": { "states": { "idle": [ 0, 0 ], "fall": [ 200, 0 ] } }
}
)";

constexpr ImVec2 cCanvas{ 800, 600 };

struct Ui
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "hero.anim", string( cGraph ) },
                                 { "other.anim", string( cGraph ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    ImGuiContextScope mContext;
    Scope<Inspector> mInspector;
    // The graph whose window is drawn: the active one when none is named.
    DataDocument* mShown = nullptr;

    // `more` files beside the graphs: a level, a model.
    explicit Ui( hmap<string, string> more = {} )
    {
        mFiles.merge( more );
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
        REQUIRE( mProject->Run( "document.open", *Json::TryParse( R"({ "path": "hero.anim" })" ) ) );
        mInspector = CreateScope<Inspector>( *mProject );
        Editor::SetUpImGui( 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( cCanvas.x + 400, cCanvas.y );
        for ( int frame = 0; frame < 5; ++frame )
            Frame();
    }

    DataDocument& Data() { return *mProject->GetWorkspace().TryActiveData(); }
    DataDocument& Shown() { return mShown != nullptr ? *mShown : Data(); }
    GraphView& View() { return GraphViewOf( *mProject, Shown() ); }

    void Frame()
    {
        mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        mInspector->Settle();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( cCanvas );
        ImGui::Begin( "Viewport", nullptr, ImGuiWindowFlags_NoDecoration );
        View().Show( Shown() );
        ImGui::End();
        ImGui::SetNextWindowPos( ImVec2( cCanvas.x, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 400, cCanvas.y ) );
        ImGui::Begin( "Inspector", nullptr, ImGuiWindowFlags_NoDecoration );
        mInspector->Show();
        ImGui::End();
        ImGui::Render();
    }

    string At( string_view path )
    {
        const auto atMaybe = Data().TryAt( *FieldPath::TryParse( path ) );
        if ( not atMaybe )
            return "-";
        string text = atMaybe->Write();
        erase_if( text, []( char c ) { return c == ' ' or c == '\n'; } );
        return text;
    }

    vec2 OnScreen( string_view node )
    {
        const auto atMaybe = View().TryNodeOnScreen( node );
        REQUIRE_MESSAGE( atMaybe, node );
        return *atMaybe;
    }

    void MoveTo( vec2 at )
    {
        ImGui::GetIO().AddMousePosEvent( at.x, at.y );
        Frame();
    }

    void Button( bool down )
    {
        ImGui::GetIO().AddMouseButtonEvent( 0, down );
        Frame();
    }

    void Click( vec2 at )
    {
        MoveTo( at );
        Button( true );
        Button( false );
        Frame();
    }

    void Press( ImGuiKey key )
    {
        ImGui::GetIO().AddKeyEvent( key, true );
        Frame();
        ImGui::GetIO().AddKeyEvent( key, false );
        Frame();
    }
};
}

TEST_CASE( "The graph's canvas draws each state where the document keeps it; a node dragged is one step" )
{
    Ui ui;
    const auto at = [&]( string_view node ) {
        const auto atMaybe = ui.View().TryNodeAt( node );
        REQUIRE_MESSAGE( atMaybe, node );
        return *atMaybe;
    };
    CHECK( at( "idle" ) == vec2( 0, 0 ) );
    CHECK( at( "fall" ) == vec2( 200, 0 ) );
    // Kept nowhere, a node falls in its row and the file is not touched.
    CHECK( at( "land" ) == vec2( 520, 0 ) );
    CHECK( ui.View().TryNodeAt( "*" ) );
    CHECK( ui.View().TryNodeAt( "return" ) );
    CHECK_FALSE( ui.Data().IsDirty() );

    const vec2 from = ui.OnScreen( "fall" );
    ui.MoveTo( from );
    ui.Button( true );
    for ( int step = 1; step <= 5; ++step )
        ui.MoveTo( from + vec2( 12.0f, 8.0f ) * static_cast<f32>( step ) );
    ui.Button( false );
    ui.Frame();
    CHECK( ui.At( "editor.states.fall" ) != "[200,0]" );
    CHECK( at( "fall" ).x > 200.0f );
    REQUIRE( ui.Data().CanUndo() );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK_FALSE( ui.Data().CanUndo() );
    ui.Frame();
    ui.Frame();
    CHECK( ui.At( "editor.states.fall" ) == "[200,0]" );
    CHECK( at( "fall" ) == vec2( 200, 0 ) );
}

TEST_CASE( "A click on a graph's canvas makes its document the active one, the inspector's" )
{
    Ui ui;
    ui.mShown = &ui.Data();
    REQUIRE( ui.mProject->Run( "document.open", *Json::TryParse( R"({ "path": "other.anim" })" ) ) );
    CHECK( ui.Data().GetPath().View() == "other.anim" );
    ui.Frame();
    ui.Frame();
    ui.Click( ui.OnScreen( "fall" ) );
    CHECK( ui.Data().GetPath().View() == "hero.anim" );
    CHECK( ui.View().SelectedState() == "fall" );
    CHECK( ui.mInspector->TryWhere( "states.fall.loop" ) );
}

TEST_CASE( "A state clicked on the canvas is the inspector's to edit; Delete takes it out with its transitions" )
{
    Ui ui;
    ui.Click( ui.OnScreen( "fall" ) );
    CHECK( ui.View().SelectedState() == "fall" );
    const auto loopMaybe = ui.mInspector->TryWhere( "states.fall.loop" );
    REQUIRE( loopMaybe );
    CHECK( ui.mInspector->TryWhere( "states.fall.plays" ) );
    ui.Click( *loopMaybe );
    CHECK( ui.At( "states.fall.loop" ) == "false" );

    // Picked by the inspector after a rename, it stays selected.
    ui.View().Select( "land" );
    ui.Frame();
    ui.Frame();
    CHECK( ui.View().SelectedState() == "land" );
    CHECK( ui.mInspector->TryWhere( "states.land.loop" ) );

    ui.Click( ui.OnScreen( "fall" ) );
    ui.Press( ImGuiKey_Delete );
    ui.Frame();
    CHECK( ui.At( "states.fall" ) == "-" );
    CHECK( ui.At( "transitions" ) == R"([{"from":"land","to":"return","exit_time":0.9}])" );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( ui.At( "states.fall" ) == R"({"loop":false})" );
}

TEST_CASE( "While a game plays, the canvas lights the state an animator of its graph plays there" )
{
    // The asset ids are no one's: the level finds the files by their paths.
    Ui ui( { { "strip.glb", test::SkinnedGlb() }, { "main.scene", R"({
  "version": 1,
  "root": { "children": [ { "name": "strip", "components": {
    "transform": {},
    "model": { "asset": { "asset": "00000000000000000000000000000001", "path": "strip.glb" } },
    "animator": { "graph": { "asset": "00000000000000000000000000000002", "path": "hero.anim" } } } } ] }
})" } } );
    ui.mShown = &ui.Data();
    CHECK( ui.View().LiveState().empty() );
    // What a state may play: the clips the project's models have.
    CHECK( ui.mProject->ClipNames() == vector<string>{ "bend", "lean", "walk" } );
    REQUIRE( ui.mProject->Run( "document.open", *Json::TryParse( R"({ "path": "main.scene" })" ) ) );
    REQUIRE( ui.mProject->Run( "play.start" ) );
    PlaySession& play = *ui.mProject->TryPlaying();
    for ( int frame = 0; frame < 30; ++frame )
    {
        ui.mProject->GetApp().GetJobs().RunMainQueue();
        play.Frame( 1.0 / 60 );
    }
    ui.Frame();
    CHECK( ui.View().LiveState() == "idle" );
    CHECK( ui.View().LiveEntity() == "/strip" );

    REQUIRE( ui.mProject->Run( "play.stop" ) );
    ui.Frame();
    CHECK( ui.View().LiveState().empty() );
}
