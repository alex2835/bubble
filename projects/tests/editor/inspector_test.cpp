// The inspector in ImGui, with no window and no GPU: fields of a project's
// level shown by kind and edited with the mouse and the keys as ImGui's
// events - a drag one step however many frames it takes, a script's props
// from its file, the ones set in the scene kept apart.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/ui/ui.hpp"
#include <doctest.h>
#include <imgui_internal.h>

using namespace bubble;

namespace
{
constexpr string_view cProject =
    R"({ "version": 1, "name": "inspect", "start_level": { "path": "main.scene" }, "actions": {} })";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": {
    "children": [
      {
        "name": "lamp",
        "components": {
          "transform": { "position": [1, 2, 3] },
          "point_light": { "color": [1, 1, 1, 1], "intensity": 5, "range": 10 },
          "script": { "asset": { "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "path": "glow.luau" } }
        }
      },
      { "name": "sun", "components": { "directional_light": { "shadows": true } } },
      { "name": "caption", "components": { "ui_text": { "text": "hello" } } },
      { "name": "lantern",
        "prefab": { "asset": "1a7e1a7e1a7e1a7e1a7e1a7e1a7e1a7e", "path": "lantern.prefab" },
        "set": { "0": { "point_light.intensity": 9 } } }
    ]
  }
})";

constexpr string_view cScript = R"(local P = props {
    speed = prop( 40, { min = 0, max = 100 } ),
    label = "warm",
}

type Self = typeof( P ) & { entity: entity }

function on_update( self: Self, dt: number )
end
)";

constexpr string_view cScriptImport =
    R"({ "version": 1, "asset": "5c0a1e5c0a1e5c0a1e5c0a1e5c0a1e5c", "settings": {} })";

struct Ui
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "glow.luau", string( cScript ) },
                                 { "glow.luau.import", string( cScriptImport ) },
                                 { "lantern.prefab", R"({
  "version": 1,
  "asset": "1a7e1a7e1a7e1a7e1a7e1a7e1a7e1a7e",
  "next_id": 1,
  "root": { "id": 0, "components": { "transform": {}, "point_light": { "intensity": 2, "range": 4 } } }
})" } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    ImGuiContextScope mContext;
    Scope<Inspector> mInspector;

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
        mInspector = CreateScope<Inspector>( *mProject );
        Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 500, 900 );
        Frame();
    }

    Document& GetDocument() { return *mProject->GetWorkspace().TryActive(); }
    Scene& GetScene() { return GetDocument().SceneOutsideHistory(); }
    Entity At( string_view path ) { return *TryResolve( GetScene(), EntityPath{ *Text::TryFrom( path ) } ); }

    // Selected, and shown whole: a script's file loads on Jobs' pool and
    // comes back through the main queue, its props then pushing the rows
    // under them down.
    void Select( string_view path )
    {
        REQUIRE( mProject->Run( "selection.set", *Json::TryParse( format( R"({{ "entities": [ "{}" ] }})", path ) ) ) );
        Frame();
        if ( GetScene().Has<Script>( At( path ) ) )
            for ( int frame = 0; frame < 100 and not mInspector->TryWhere( "script.props.speed" ); ++frame )
                Frame();
    }

    void Frame()
    {
        mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        mInspector->Settle();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 500, 900 ) );
        ImGui::Begin( "Inspector" );
        mInspector->Show();
        ImGui::End();
        ImGui::Render();
    }

    vec2 Where( string_view field )
    {
        const auto whereMaybe = mInspector->TryWhere( field );
        REQUIRE_MESSAGE( whereMaybe, field );
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

    // Held down on a field and taken `across` units right, a step a frame.
    void Drag( string_view field, f32 across, int steps = 5 )
    {
        const vec2 from = Where( field );
        MoveTo( from );
        Button( true );
        for ( int step = 1; step <= steps; ++step )
            MoveTo( from + vec2( across * static_cast<f32>( step ) / static_cast<f32>( steps ), 0.0f ) );
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

TEST_CASE( "The inspector shows a component's fields by kind, and a drag across frames is one step" )
{
    Ui ui;
    ui.Select( "/lamp" );
    const Entity lamp = ui.At( "/lamp" );
    CHECK( ui.mInspector->TryWhere( "transform.position" ) );
    CHECK( ui.mInspector->TryWhere( "point_light.color" ) );
    CHECK( ui.mInspector->TryWhere( "script.asset" ) );

    ui.Drag( "point_light.intensity", 100.0f );
    const f32 dragged = ui.GetScene().Get<PointLight>( lamp ).mIntensity;
    CHECK( dragged > 5.5f );
    // However many frames it took, one undo takes it back.
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( ui.GetScene().Get<PointLight>( lamp ).mIntensity == doctest::Approx( 5.0f ) );
    CHECK( not ui.GetDocument().IsDirty() );

    // A bool is a box to tick.
    ui.Select( "/sun" );
    const Entity sun = ui.At( "/sun" );
    ui.MoveTo( ui.Where( "directional_light.shadows" ) );
    ui.Button( true );
    ui.Button( false );
    CHECK( not ui.GetScene().Get<DirectionalLight>( sun ).mShadows );
}

TEST_CASE( "The inspector renames the entity when its name is left, as one step" )
{
    Ui ui;
    ui.Select( "/sun" );
    ui.MoveTo( ui.Where( "name" ) );
    ui.Button( true );
    ui.Button( false );
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, true );
    ui.Press( ImGuiKey_A );
    ImGui::GetIO().AddKeyEvent( ImGuiMod_Ctrl, false );
    ImGui::GetIO().AddInputCharactersUTF8( "солнце" );
    ui.Frame();
    // Not yet: the name is taken when the field is left.
    CHECK( NameOf( ui.GetScene(), ui.At( "/sun" ) ).String() == "sun" );
    ui.Press( ImGuiKey_Enter );
    CHECK( TryResolve( ui.GetScene(), EntityPath{ *Text::TryFrom( "/солнце" ) } ) );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( TryResolve( ui.GetScene(), EntityPath{ *Text::TryFrom( "/sun" ) } ) );
}

TEST_CASE( "A script's props come from its file; one edited is set in the scene, the rest stay its defaults" )
{
    Ui ui;
    ui.Select( "/lamp" );
    REQUIRE( ui.mInspector->TryWhere( "script.props.speed" ) );
    CHECK( ui.mInspector->TryWhere( "script.props.label" ) );

    ui.Drag( "script.props.speed", 60.0f );
    const Value& props = ui.GetScene().Get<Script>( ui.At( "/lamp" ) ).mProps;
    const Value::Record& set = *props.TryAs<Value::Record>();
    REQUIRE( set.size() == 1 );
    CHECK( set[0].mName == "speed"_n );
    CHECK( *set[0].mValue.TryAs<f64>() > 40.0 );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( ui.GetScene().Get<Script>( ui.At( "/lamp" ) ).mProps.TryAs<Value::Record>()->empty() );
}

TEST_CASE( "On a prefab's instance the inspector tells apart the fields it has otherwise than its prefab" )
{
    Ui ui;
    ui.Select( "/lantern" );
    REQUIRE( ui.mInspector->TryWhere( "point_light.intensity" ) );
    CHECK( ui.mInspector->Marked( "point_light.intensity" ) );
    CHECK_FALSE( ui.mInspector->Marked( "point_light.range" ) );
    CHECK_FALSE( ui.mInspector->Marked( "transform.position" ) );

    // Set back to the prefab's, it follows the prefab again.
    REQUIRE( ui.mProject->Run(
        "property.set",
        *Json::TryParse(
            R"({ "entities": [ "/lantern" ], "component": "point_light", "path": "intensity", "value": 2 })" ) ) );
    ui.Frame();
    CHECK_FALSE( ui.mInspector->Marked( "point_light.intensity" ) );
    // A plain entity has no prefab to differ from.
    ui.Select( "/lamp" );
    CHECK_FALSE( ui.mInspector->Marked( "point_light.intensity" ) );
}

TEST_CASE( "A name field that says the names it takes offers them" )
{
    Ui ui;
    ui.Select( "/caption" );
    const vec2 align = ui.Where( "ui_text.align" );
    ui.MoveTo( align );
    ui.Button( true );
    ui.Button( false );
    ui.Frame();
    // The list opens under the field: start, center, end.
    const ImGuiStyle& style = ImGui::GetStyle();
    const f32 line = ImGui::GetTextLineHeightWithSpacing();
    const f32 top = align.y + ImGui::GetFrameHeight() * 0.5f + style.WindowPadding.y;
    ui.MoveTo( vec2( align.x, top + line * 1.5f ) );
    ui.Button( true );
    ui.Button( false );
    ui.Frame();
    CHECK( ui.GetScene().Get<UiText>( ui.At( "/caption" ) ).mAlign == "center"_n );
}
