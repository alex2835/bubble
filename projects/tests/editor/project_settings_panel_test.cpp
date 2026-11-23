// Project Settings: project.bubble open as a data document and edited in its
// panel with the mouse and the keys as ImGui's events - an action added,
// bound, renamed in its place, its scale typed, a binding taken out -, saved,
// and read again by the project.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/project_settings_panel.hpp"
#include "bubble/types/algorithm.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({
  "version": 1,
  "name": "settings",
  "start_level": { "path": "main.scene" },
  "actions": { "jump": [ { "key": "space" } ], "fire": [ { "mouse_button": "left" } ] }
})";

struct Ui
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", R"({ "version": 1, "root": {} })" },
                                 { "cave.scene", R"({ "version": 1, "root": {} })" } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    ImGuiContextScope mContext;
    Scope<ProjectSettingsPanel> mPanel;

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
        REQUIRE( mProject->Run( "document.open", *Json::TryParse( R"({ "path": "project.bubble" })" ) ) );
        mPanel = CreateScope<ProjectSettingsPanel>( *mProject );
        Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 600, 700 );
        Frame();
    }

    DataDocument& Settings() { return *mProject->GetWorkspace().TryActiveData(); }
    string Data() { return Settings().GetData().Write(); }

    void Frame()
    {
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 600, 700 ) );
        ImGui::Begin( "Project Settings" );
        mPanel->Show( Settings() );
        ImGui::End();
        ImGui::Render();
    }

    vec2 Where( string_view what )
    {
        const auto whereMaybe = mPanel->TryWhere( what );
        REQUIRE_MESSAGE( whereMaybe, what );
        return *whereMaybe;
    }

    void Click( string_view what )
    {
        const vec2 at = Where( what );
        ImGui::GetIO().AddMousePosEvent( at.x, at.y );
        Frame();
        ImGui::GetIO().AddMouseButtonEvent( 0, true );
        Frame();
        ImGui::GetIO().AddMouseButtonEvent( 0, false );
        Frame();
        // What the click changed, shown.
        Frame();
    }

    void Press( ImGuiKey key )
    {
        ImGui::GetIO().AddKeyEvent( key, true );
        Frame();
        ImGui::GetIO().AddKeyEvent( key, false );
        Frame();
    }

    // The field's text replaced by `text`, and left with Enter.
    void Type( string_view what, string_view text )
    {
        Click( what );
        ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, true );
        Press( ImGuiKey_A );
        ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, false );
        ImGui::GetIO().AddInputCharactersUTF8( string( text ).c_str() );
        Frame();
        Press( ImGuiKey_Enter );
        Frame();
    }
};
}

TEST_CASE( "The names an input takes are the engine's, a touch none" )
{
    const vector<string_view> keys = InputNames( "key" );
    CHECK( ranges::contains( keys, "space" ) );
    CHECK( ranges::contains( keys, "a" ) );
    CHECK( InputNames( "mouse_axis" ) == vector<string_view>{ "x", "y", "wheel" } );
    CHECK( InputNames( "gamepad_button" ).size() == cGamepadButtons );
    CHECK( InputNames( "touch" ).empty() );
}

TEST_CASE( "Project Settings add an action, bind it, rename it in its place, and are saved for the project to read" )
{
    Ui ui;
    CHECK( IsProjectSettings( ui.Settings() ) );
    ui.Type( "name", "renamed" );
    CHECK( *ui.Settings().TryAt( *FieldPath::TryParse( "name" ) )->TryAs<string>() == "renamed" );

    ui.Click( "add_action" );
    CHECK( ui.Settings().TryAt( *FieldPath::TryParse( "actions.new_action" ) )->TryAs<Json::Array>()->empty() );
    ui.Click( "new_action.add_binding" );
    REQUIRE( ui.Settings().TryAt( *FieldPath::TryParse( "actions.new_action[1].key" ) ) );

    // Renamed in its place, between fire and nothing.
    ui.Type( "new_action.name", "dash" );
    const string data = ui.Data();
    CHECK_FALSE( data.contains( "new_action" ) );
    CHECK( data.find( "\"fire\"" ) < data.find( "\"dash\"" ) );
    CHECK( ui.mPanel->TryWhere( "dash[1].scale" ) );

    // A name that is not snake_case, or taken, is refused.
    ui.Type( "dash.name", "jump" );
    ui.Type( "dash.name", "Dash!" );
    CHECK( ui.Settings().TryAt( *FieldPath::TryParse( "actions.dash" ) ) );

    ui.Type( "dash[1].scale", "-1" );
    CHECK( *ui.Settings().TryAt( *FieldPath::TryParse( "actions.dash[1].scale" ) )->TryAs<f64>() == -1.0 );
    ui.Click( "jump[1].remove" );
    CHECK( ui.Settings().TryAt( *FieldPath::TryParse( "actions.jump" ) )->TryAs<Json::Array>()->empty() );

    // Undone as any data document; saved, the project reads it again.
    ui.Settings().Undo();
    CHECK_FALSE( ui.Settings().TryAt( *FieldPath::TryParse( "actions.jump" ) )->TryAs<Json::Array>()->empty() );
    REQUIRE( ui.mProject->Run( "document.save" ) );
    CHECK_FALSE( ui.Settings().IsDirty() );
    ui.mProject->Refresh();
    CHECK( ActionNames( ui.mProject->GetSettings().mActions ) == vector<string>{ "jump", "fire", "dash" } );
    CHECK( ui.mProject->GetSettings().mName == "renamed" );
}
