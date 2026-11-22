// The editor whole, its frames as the window runs them, on the GPU and with
// no window: what crosses its panels - a game stopped in the middle of a
// frame by a key, the menu or the game's tab.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/play_session.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>
#include <imgui.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({
  "version": 1,
  "name": "whole",
  "start_level": { "path": "main.scene" },
  "actions": { "jump": [ { "key": "space" } ] }
})";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": { "children": [
    { "name": "eye", "components": { "transform": { "position": [ 0, 1, 5 ] }, "camera": {} } },
    { "name": "crate", "components": { "transform": {} } }
  ] }
})";

struct Whole
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) }, { "main.scene", string( cLevel ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    Scope<Gpu> mGpu = StartGpu();
    Scope<Renderer> mRenderer;
    Preferences mPreferences{ nullopt };
    ImGuiContextScope mContext;
    Scope<Editor> mEditor;

    Whole()
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
        Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 1280, 720 );
        mEditor = CreateScope<Editor>( *mProject, mPreferences, *mGpu, *mRenderer, []( const OsPath& ) {} );
    }

    ~Whole()
    {
        // The pictures go before the Gpu does.
        mEditor.reset();
        mRenderer.reset();
    }

    void Frame()
    {
        mGpu->Poll();
        mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        mEditor->Frame( 1.0 / 60 );
        ImGui::Render();
        mEditor->Draw();
    }

    void Frames( int count )
    {
        for ( int frame = 0; frame < count; ++frame )
            Frame();
    }
};
}

TEST_CASE( "Ctrl+P stops the game in the middle of the editor's frame, and the frame goes on without it" )
{
    Whole whole;
    whole.Frames( 3 );
    Document& level = *whole.mProject->GetWorkspace().TryActive();
    REQUIRE( whole.mProject->Run( "play.start" ) );
    whole.Frames( 5 );
    REQUIRE( whole.mProject->TryPlaying() );

    // Ctrl+P comes before the viewport, which shows what is there then.
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, true );
    ImGui::GetIO().AddKeyEvent( ImGuiKey_P, true );
    whole.Frame();
    ImGui::GetIO().AddKeyEvent( ImGuiKey_P, false );
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, false );
    whole.Frames( 3 );
    CHECK_FALSE( whole.mProject->TryPlaying() );
    CHECK( &*whole.mProject->GetWorkspace().TryActive() == &level );
    CHECK_FALSE( level.IsDirty() );

    // And again, from the start.
    REQUIRE( whole.mProject->Run( "play.start" ) );
    whole.Frames( 3 );
    REQUIRE( whole.mProject->Run( "play.stop" ) );
    whole.Frames( 3 );
    CHECK_FALSE( whole.mProject->TryPlaying() );
}
