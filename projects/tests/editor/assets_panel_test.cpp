// The assets panel and putting assets in a scene: a level opened by a
// double click, a file renamed in place with its .import, a script dragged
// onto the inspector's field, a prefab placed - in ImGui with no window and
// no GPU, the mouse and the keys as its events.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab_link.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cProject =
    R"({ "version": 1, "name": "assets", "start_level": { "path": "main.scene" }, "actions": {} })";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "lamp",
        "components": {
          "transform": {},
          "script": { "asset": { "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "path": "glow.luau" } }
        }
      }
    ]
  }
})";

constexpr string_view cScript = R"(local P = props {}

type Self = typeof( P ) & { entity: entity }

function on_update( self: Self, dt: number )
end
)";

constexpr string_view cGlowImport = R"({ "version": 1, "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "settings": {} })";
constexpr string_view cToolImport = R"({ "version": 1, "asset": "7001700170017001700170017001700a", "settings": {} })";

constexpr string_view cDoor = R"({
  "version": 1,
  "next_id": 2,
  "root": { "id": 0, "components": { "transform": {} }, "children": [ { "id": 1, "name": "leaf" } ] }
})";

struct Ui
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "cave.scene", R"({ "version": 1, "root": {} })" },
                                 { "glow.luau", string( cScript ) },
                                 { "glow.luau.import", string( cGlowImport ) },
                                 { "tool.luau", string( cScript ) },
                                 { "tool.luau.import", string( cToolImport ) },
                                 { "door.prefab", string( cDoor ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    ImGuiContextScope mContext;
    Scope<AssetsPanel> mAssets;
    Scope<Inspector> mInspector;
    vector<OsPath> mOpened;

    Ui()
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
        mAssets = CreateScope<AssetsPanel>( *mProject, [this]( const OsPath& path ) { mOpened.push_back( path ); } );
        mInspector = CreateScope<Inspector>( *mProject );
        Editor::SetUpImGui( 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 900, 600 );
        Frame();
    }

    Document& GetDocument() { return *mProject->GetWorkspace().TryActive(); }

    void Frame()
    {
        mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        mInspector->Settle();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 450, 600 ) );
        ImGui::Begin( "Assets" );
        mAssets->Show();
        ImGui::End();
        ImGui::SetNextWindowPos( ImVec2( 450, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 450, 600 ) );
        ImGui::Begin( "Inspector" );
        mInspector->Show();
        ImGui::End();
        ImGui::Render();
    }

    vec2 Row( string_view path )
    {
        const auto whereMaybe = mAssets->TryWhere( path );
        REQUIRE_MESSAGE( whereMaybe, path );
        return *whereMaybe;
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

TEST_CASE( "The assets panel opens a level at a double click, and hands a script to the system" )
{
    Ui ui;
    ui.MoveTo( ui.Row( "cave.scene" ) );
    for ( int click = 0; click < 2; ++click )
    {
        ui.Button( true );
        ui.Button( false );
    }
    CHECK( ui.GetDocument().TryPath()->View() == "cave.scene" );
    // A project in memory has no folder for the system to open things in.
    CHECK( ui.mOpened.empty() );
}

TEST_CASE( "The assets panel renames a file in place with F2, its .import going along" )
{
    Ui ui;
    ui.Click( ui.Row( "tool.luau" ) );
    ui.Press( ImGuiKey_F2 );
    ui.Frame();
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, true );
    ui.Press( ImGuiKey_A );
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, false );
    ImGui::GetIO().AddInputCharactersUTF8( "hammer.luau" );
    ui.Frame();
    ui.Press( ImGuiKey_Enter );
    CHECK( ui.mFiles.contains( "hammer.luau" ) );
    CHECK( ui.mFiles.contains( "hammer.luau.import" ) );
    CHECK( not ui.mFiles.contains( "tool.luau" ) );
}

TEST_CASE( "A script dragged from the assets panel onto the inspector's field is set there, one step" )
{
    Ui ui;
    REQUIRE( ui.mProject->Run( "selection.set", *Json::TryParse( R"({ "entities": [ "/lamp" ] })" ) ) );
    ui.Frame();
    const auto fieldMaybe = ui.mInspector->TryWhere( "script.asset" );
    REQUIRE( fieldMaybe );
    const vec2 from = ui.Row( "tool.luau" );
    ui.MoveTo( from );
    ui.Button( true );
    for ( int step = 1; step <= 8; ++step )
        ui.MoveTo( from + ( *fieldMaybe - from ) * ( static_cast<f32>( step ) / 8 ) );
    ui.Button( false );
    ui.Frame();
    const Entity lamp = *TryResolve( ui.GetDocument().GetScene(), EntityPath{ *Text::TryFrom( "/lamp" ) } );
    CHECK( ui.GetDocument().GetScene().Get<Script>( lamp ).mAsset.ToString() == "7001700170017001700170017001700a" );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( ui.GetDocument().GetScene().Get<Script>( lamp ).mAsset.ToString() == "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c" );
}

TEST_CASE( "A prefab placed in a scene is its instance where it was put, one step; a script is no such asset" )
{
    Ui ui;
    Document& document = ui.GetDocument();
    const size_t before = document.GetScene().Count();
    const auto placedMaybe = TryPlaceAsset( *ui.mProject, *AssetPath::TryFrom( "door.prefab" ),
                                            EntityPath{ *Text::TryFrom( "/lamp" ) }, vec3( 1, 0, 2 ) );
    REQUIRE_MESSAGE( placedMaybe, placedMaybe.error() );
    const Scene& scene = document.GetScene();
    const Entity door = document.GetSelection().Active();
    CHECK( PathOf( scene, door ).mPath.String() == "/lamp/door" );
    CHECK( scene.Has<PrefabInstance>( door ) );
    CHECK( scene.Get<Transform>( door ).mPosition == vec3( 1, 0, 2 ) );
    CHECK( ChildrenOf( scene, door ).size() == 1 );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( scene.Count() == before );

    CHECK( TryPlaceAsset( *ui.mProject, *AssetPath::TryFrom( "glow.luau" ), EntityPath{ *Text::TryFrom( "/" ) } )
               .error() == "glow.luau: a prefab or a model goes into a scene, not this" );
    CHECK( scene.Count() == before );
    CHECK( not document.IsDirty() );
}
