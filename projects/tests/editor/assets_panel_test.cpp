// The assets panel and putting assets in a scene: a level opened by a
// double click, a file renamed in place with its .import, a script dragged
// onto the inspector's field, a prefab placed - in ImGui with no window and
// no GPU, the mouse and the keys as its events.
#include "bubble/audio/audio.hpp"
#include "bubble/data/png.hpp"
#include "bubble/editor/assets_panel.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/import/import_file.hpp"
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
                                 { "door.prefab", string( cDoor ) },
                                 { "hero.anim", R"({ "entry": "idle", "states": { "idle": {} } })" },
                                 { "sky.png", EncodePng( array<u8, 4>{ 90, 160, 255, 255 }, ivec2( 1, 1 ) ) } };
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
        if ( auto pickedMaybe = mAssets->TakePicked() )
            mInspector->Inspect( std::move( *pickedMaybe ) );
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

    void DoubleClick( vec2 at )
    {
        MoveTo( at );
        for ( int click = 0; click < 2; ++click )
        {
            Button( true );
            Button( false );
        }
    }
};
}

TEST_CASE( "The assets panel opens a level at a double click, and hands a script to the system" )
{
    Ui ui;
    ui.DoubleClick( ui.Row( "cave.scene" ) );
    CHECK( ui.GetDocument().TryPath()->View() == "cave.scene" );
    ui.DoubleClick( ui.Row( "glow.luau" ) );
    // A project in memory has no folder for the system to open things in.
    CHECK( ui.mOpened.empty() );
}

TEST_CASE( "The assets panel opens an animation graph at a double click as a document of its own" )
{
    Ui ui;
    ui.DoubleClick( ui.Row( "hero.anim" ) );
    const auto dataMaybe = ui.mProject->GetWorkspace().TryActiveData();
    REQUIRE( dataMaybe );
    CHECK( dataMaybe->GetPath().View() == "hero.anim" );
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

TEST_CASE( "A file picked in the assets panel shows how it imports; a setting changed is written to its .import" )
{
    Ui ui;
    ui.Click( ui.Row( "sky.png" ) );
    ui.Frame();
    const auto srgbMaybe = ui.mInspector->TryWhere( "import.srgb" );
    REQUIRE( srgbMaybe );
    CHECK( ui.mInspector->TryWhere( "import.mips" ) );
    ui.Click( *srgbMaybe );
    const auto fileMaybe = TryParseImportFile( ui.mFiles.at( "sky.png.import" ) );
    REQUIRE( fileMaybe );
    const auto srgbSetMaybe = fileMaybe->mSettings.TryFind( "srgb" );
    REQUIRE( srgbSetMaybe );
    REQUIRE( srgbSetMaybe->TryAs<bool>() );
    CHECK_FALSE( *srgbSetMaybe->TryAs<bool>() );

    // The documents' selection changed, the inspector shows it again.
    REQUIRE( ui.mProject->Run( "selection.set", *Json::TryParse( R"({ "entities": [ "/lamp" ] })" ) ) );
    ui.Frame();
    CHECK( not ui.mInspector->TryWhere( "import.srgb" ) );
}

TEST_CASE( "A folder made in the panel is shown empty and gone into; a new asset goes in it" )
{
    Ui ui;
    CHECK_FALSE( ui.mAssets->MakeFolder( ".hidden" ) );
    CHECK_FALSE( ui.mAssets->MakeFolder( "a/b" ) );
    REQUIRE( ui.mAssets->MakeFolder( "effects" ) );
    ui.Frame();
    CHECK( ui.mAssets->Folder() == "effects" );
    ui.mAssets->SetFolder( "" );
    ui.Frame();
    CHECK( ui.mAssets->TryWhere( "effects/" ) );
    REQUIRE( ui.mProject->Run( "asset.create", *Json::TryParse( R"({ "path": "effects/smoke.mat" })" ) ) );
    CHECK( ui.mFiles.contains( "effects/smoke.mat" ) );
}
