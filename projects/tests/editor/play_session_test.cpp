// Play in the editor: the scene shown copied into a document of its own and
// played by a world of every module - stepped, paused, stepped once, its
// input the project's actions -, the document it came from untouched, the
// copy thrown away at Stop. No window and no GPU: the operators and the
// session alone.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/editor/play_session.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/world/game.hpp"
#include "bubble/world/transform.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({
  "version": 1,
  "name": "played",
  "start_level": { "path": "main.scene" },
  "actions": { "jump": [ { "key": "space" } ] }
})";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": { "children": [
    { "name": "crate", "components": { "transform": { "position": [ 0, 2, 0 ] } } },
    { "name": "counter", "components": {
      "script": { "asset": { "asset": "00000000000000000000000000000003", "path": "counter.luau" } } } }
  ] }
})";

constexpr string_view cCounter = R"(local P = props { step = 2 }
local L = locals { ticks = 0 }
type Self = typeof( P ) & typeof( L ) & { entity: entity }

function on_fixed_update( self: Self, dt: number )
    self.ticks += self.step
end
)";

// The level a game goes on to.
constexpr string_view cSecond = R"({
  "version": 1,
  "asset": "5ec0d0005ec0d0005ec0d0005ec0d000",
  "root": { "children": [ { "name": "gate", "components": { "transform": {} } } ] }
})";

struct Fixture
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "counter.luau", string( cCounter ) },
                                 { "second.scene", string( cSecond ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;

    Fixture()
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
    }

    Workspace& GetWorkspace() { return mProject->GetWorkspace(); }

    expected<void, string> Run( string_view id, string_view args = "null" )
    {
        return mProject->GetOperators().TryRun( GetWorkspace(), id, *Json::TryParse( args ) );
    }

    static vec3 CrateAt( const Document& document )
    {
        const Scene& scene = document.GetScene();
        return scene.Get<Transform>( *TryResolve( scene, EntityPath{ *Text::TryFrom( "/crate" ) } ) ).mPosition;
    }
};
}

TEST_CASE( "Play copies the scene shown into a document of its own, played; Stop throws it away" )
{
    Fixture f;
    Document& level = *f.GetWorkspace().TryActive();
    REQUIRE( f.Run( "play.start" ) );
    const auto playMaybe = f.mProject->TryPlaying();
    REQUIRE( playMaybe );
    Document& played = playMaybe->Played();
    CHECK( played.IsPlayed() );
    CHECK( &*f.GetWorkspace().TryActive() == &played );
    CHECK( playMaybe->Title() == "main.scene" );
    CHECK( f.Run( "play.start" ).error() == "play.start: a game is playing already: play.stop ends it" );

    // A world of every module, stepping: 60 steps a second.
    World& world = playMaybe->GetWorld();
    CHECK_FALSE( world.Editing() );
    for ( int frame = 0; frame < 10; ++frame )
        playMaybe->Frame( 1.0 / 60 );
    CHECK( world.Steps() >= 9 );

    // Paused, frames go on and steps do not; a step is one.
    REQUIRE( f.Run( "play.pause" ) );
    const u64 steps = world.Steps();
    playMaybe->Frame( 0.5 );
    CHECK( world.Steps() == steps );
    REQUIRE( f.Run( "play.step" ) );
    CHECK( world.Steps() == steps + 1 );
    REQUIRE( f.Run( "play.pause", R"({ "paused": false })" ) );
    CHECK( f.Run( "play.step" ).error() == "play.step: a game steps one step at a time while it is paused" );

    // The copy is edited as any document, the level not; it is not saved.
    REQUIRE( f.Run(
        "property.set",
        R"({ "entities": [ "/crate" ], "component": "transform", "path": "position", "value": [ 5, 2, 0 ] })" ) );
    CHECK( Fixture::CrateAt( played ) == vec3( 5, 2, 0 ) );
    CHECK( Fixture::CrateAt( level ) == vec3( 0, 2, 0 ) );
    CHECK( f.Run( "document.save" ).error() ==
           "document.save: a game playing is not saved: stop it, and edit the level it was played from" );
    CHECK( f.Run( "document.close" ).error() == "document.close: a game is playing in it: play.stop ends it" );

    REQUIRE( f.Run( "play.stop" ) );
    CHECK_FALSE( f.mProject->TryPlaying() );
    CHECK( f.GetWorkspace().Count() == 1 );
    CHECK( &*f.GetWorkspace().TryActive() == &level );
    CHECK_FALSE( level.IsDirty() );
    CHECK( Fixture::CrateAt( level ) == vec3( 0, 2, 0 ) );
    CHECK( f.Run( "play.stop" ).error() == "play.stop: no game is playing: play.start begins one" );
}

TEST_CASE( "A game played takes the project's actions from the input it is handed, and lets go when turned away" )
{
    Fixture f;
    REQUIRE( f.Run( "play.start" ) );
    PlaySession& play = *f.mProject->TryPlaying();
    const Name jump = *Name::TryFrom( "jump" );
    play.Handle( KeyDown{ .mKey = Key::Space } );
    play.Frame( 1.0 / 60 );
    CHECK( play.GetWorld().GetActions().Down( jump ) );
    play.Frame( 1.0 / 60 );
    CHECK( play.GetWorld().GetActions().Down( jump ) );
    // The editor took the keys back: what was held comes up.
    play.Release();
    play.Frame( 1.0 / 60 );
    CHECK_FALSE( play.GetWorld().GetActions().Down( jump ) );
}

TEST_CASE( "While a game plays, the inspector shows a script's self as the game has it" )
{
    Fixture f;
    ImGuiContextScope context;
    Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetIO().DisplaySize = ImVec2( 500, 900 );
    Inspector inspector( *f.mProject );
    const auto frame = [&] {
        f.mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        ImGui::Begin( "Inspector" );
        inspector.Show();
        ImGui::End();
        ImGui::Render();
    };
    REQUIRE( f.Run( "play.start" ) );
    PlaySession& play = *f.mProject->TryPlaying();
    REQUIRE( f.Run( "selection.set", R"({ "entities": [ "/counter" ] })" ) );
    for ( int at = 0; at < 30; ++at )
    {
        f.mProject->GetApp().GetJobs().RunMainQueue();
        play.Frame( 1.0 / 60 );
    }
    frame();
    CHECK( inspector.TryWhere( "script.self.ticks" ) );
    CHECK( inspector.TryWhere( "script.self.step" ) );
    CHECK_FALSE( inspector.TryWhere( "script.self.entity" ) );
    // The level's own document shows no game.
    REQUIRE( f.Run( "play.stop" ) );
    REQUIRE( f.Run( "selection.set", R"({ "entities": [ "/counter" ] })" ) );
    frame();
    CHECK_FALSE( inspector.TryWhere( "script.self.ticks" ) );
}

TEST_CASE( "A level a game asks for is played in the same document, a world of its own over it" )
{
    Fixture f;
    Document& level = *f.GetWorkspace().TryActive();
    REQUIRE( f.Run( "play.start" ) );
    PlaySession& play = *f.mProject->TryPlaying();
    Document& played = play.Played();
    play.Frame( 1.0 / 60 );
    REQUIRE( f.Run( "selection.set", R"({ "entities": [ "/crate" ] })" ) );

    // Nothing asked: nothing happens.
    World& before = play.GetWorld();
    play.LoadAskedLevel();
    CHECK( &play.GetWorld() == &before );

    // One the project lacks is logged; the game goes on where it is.
    before.Get<Game>().LoadLevel( AssetId::Generate() );
    play.LoadAskedLevel();
    CHECK( &play.GetWorld() == &before );
    CHECK( TryResolve( played.GetScene(), EntityPath{ *Text::TryFrom( "/crate" ) } ) );

    before.Get<Game>().LoadLevel( *AssetId::TryParse( "5ec0d0005ec0d0005ec0d0005ec0d000" ) );
    play.LoadAskedLevel();
    CHECK( &play.Played() == &played );
    CHECK( play.Title() == "second.scene" );
    CHECK( play.GetWorld().Level() == *AssetId::TryParse( "5ec0d0005ec0d0005ec0d0005ec0d000" ) );
    CHECK( TryResolve( played.GetScene(), EntityPath{ *Text::TryFrom( "/gate" ) } ) );
    CHECK_FALSE( TryResolve( played.GetScene(), EntityPath{ *Text::TryFrom( "/crate" ) } ) );
    CHECK( played.GetSelection().Empty() );
    CHECK_FALSE( played.GetHistory().CanUndo() );
    for ( int frame = 0; frame < 5; ++frame )
        play.Frame( 1.0 / 60 );
    CHECK( play.GetWorld().Steps() >= 4 );

    // Stopped, the level played from is as it was.
    REQUIRE( f.Run( "play.stop" ) );
    CHECK( &*f.GetWorkspace().TryActive() == &level );
    CHECK( Fixture::CrateAt( level ) == vec3( 0, 2, 0 ) );
}
