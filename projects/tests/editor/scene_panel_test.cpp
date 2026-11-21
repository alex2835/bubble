// What the scene panel's gestures ask of the operators: clicks with ctrl and
// shift as a file manager has them, and drops into a row or beside it - run
// on a workspace, and the selection and the tree checked after; then the
// panel itself in ImGui, its gestures given as events.
#include "bubble/audio/audio.hpp"
#include "bubble/editing/operators.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/scene_panel.hpp"
#include "bubble/scene/hierarchy.hpp"
#include <doctest.h>
#include <imgui_internal.h>

using namespace bubble;

namespace
{
Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

// /a with /a/one and /a/two under it, then /b and /c.
struct Fixture
{
    hmap<string, string> mFiles;
    Reflection mReflection;
    ComponentTypes mComponentTypes{ mReflection };
    Operators mOperators;
    AssetCatalog mAssets;
    Workspace mWorkspace{ mComponentTypes, ProjectFiles::InMemory( mFiles ), mAssets };
    Document& mDocument = mWorkspace.Open( Tree() );

    Fixture() { AddBuiltinOperators( mOperators ); }

    static Scene Tree()
    {
        Scene scene;
        const Entity a = CreateChild( scene, scene.Root(), Make( "a" ) );
        CreateChild( scene, a, Make( "one" ) );
        CreateChild( scene, a, Make( "two" ) );
        CreateChild( scene, scene.Root(), Make( "b" ) );
        CreateChild( scene, scene.Root(), Make( "c" ) );
        return scene;
    }

    const Scene& GetScene() const { return mDocument.GetScene(); }

    Entity At( string_view path ) const { return *TryResolve( GetScene(), EntityPath{ Make( path ) } ); }

    // The rows as the panel shows them, every node open.
    vector<Entity> Rows() const { return { At( "/a" ), At( "/a/one" ), At( "/a/two" ), At( "/b" ), At( "/c" ) }; }

    void Run( const OperatorCall& call )
    {
        const auto ranMaybe = mOperators.TryRun( mWorkspace, call.mId, call.mArgs );
        REQUIRE_MESSAGE( ranMaybe, ranMaybe.error() );
    }

    void Click( string_view path, bool ctrl = false, bool shift = false )
    {
        Run( ClickCall( GetScene(), mDocument.GetSelection(), Rows(), At( path ), ctrl, shift ) );
    }

    vector<string> Selected() const
    {
        vector<string> names;
        for ( const Entity entity : mDocument.GetSelection().All() )
            names.push_back( PathOf( GetScene(), entity ).mPath.String() );
        return names;
    }

    // The tree, a path a line in its order.
    vector<string> Paths() const
    {
        vector<string> paths;
        const function<void( Entity )> walk = [&]( Entity entity ) {
            for ( const Entity child : ChildrenOf( GetScene(), entity ) )
            {
                paths.push_back( PathOf( GetScene(), child ).mPath.String() );
                walk( child );
            }
        };
        walk( GetScene().Root() );
        return paths;
    }

    void Drop( vector<string_view> dragged, string_view target, DropPlace place )
    {
        vector<Entity> entities;
        for ( const string_view path : dragged )
            entities.push_back( At( path ) );
        const auto callMaybe = TryDropCall( GetScene(), entities, At( target ), place );
        REQUIRE( callMaybe );
        Run( *callMaybe );
    }
};
}

TEST_CASE( "A click selects a row, ctrl flips one, shift selects from the active one across the rows" )
{
    Fixture f;
    f.Click( "/a/one" );
    CHECK( f.Selected() == vector<string>{ "/a/one" } );
    f.Click( "/c", true );
    CHECK( f.Selected() == vector<string>{ "/a/one", "/c" } );
    f.Click( "/a/one", true );
    CHECK( f.Selected() == vector<string>{ "/c" } );

    // From /c up to /a/two: /a/two is active then.
    f.Click( "/a/two", false, true );
    CHECK( f.Selected() == vector<string>{ "/c", "/b", "/a/two" } );
    f.Click( "/b" );
    f.Click( "/c", false, true );
    CHECK( f.Selected() == vector<string>{ "/b", "/c" } );

    // Nothing active: shift picks the row alone.
    f.mDocument.GetSelection().Clear();
    f.Click( "/b", false, true );
    CHECK( f.Selected() == vector<string>{ "/b" } );
}

TEST_CASE( "A drop puts what is dragged into a row, or above or below it, in one step" )
{
    Fixture f;
    f.Drop( { "/c" }, "/a", DropPlace::Into );
    CHECK( f.Paths() == vector<string>{ "/a", "/a/one", "/a/two", "/a/c", "/b" } );

    // Above /a/one, out of /b's place.
    f.Drop( { "/b" }, "/a/one", DropPlace::Before );
    CHECK( f.Paths() == vector<string>{ "/a", "/a/b", "/a/one", "/a/two", "/a/c" } );

    // Below a sibling it was above: the index counts those that stay.
    f.Drop( { "/a/b" }, "/a/two", DropPlace::After );
    CHECK( f.Paths() == vector<string>{ "/a", "/a/one", "/a/two", "/a/b", "/a/c" } );

    // Two at once, out to the root above /a, in the tree's order.
    f.Drop( { "/a/c", "/a/one" }, "/a", DropPlace::Before );
    CHECK( f.Paths() == vector<string>{ "/one", "/c", "/a", "/a/two", "/a/b" } );

    // One step: undo puts it all back as it was before the last drop.
    REQUIRE( f.mOperators.TryRun( f.mWorkspace, "edit.undo" ) );
    CHECK( f.Paths() == vector<string>{ "/a", "/a/one", "/a/two", "/a/b", "/a/c" } );

    // The root takes what lands below the rows, at its end.
    f.Drop( { "/a/one" }, "/", DropPlace::Into );
    CHECK( f.Paths().back() == "/one" );
}

TEST_CASE( "A drop on what is dragged, or under it, asks for nothing" )
{
    Fixture f;
    const vector<Entity> a{ f.At( "/a" ) };
    CHECK( not TryDropCall( f.GetScene(), a, f.At( "/a" ), DropPlace::Into ) );
    CHECK( not TryDropCall( f.GetScene(), a, f.At( "/a/two" ), DropPlace::After ) );
    CHECK( not TryDropCall( f.GetScene(), {}, f.At( "/b" ), DropPlace::Into ) );
    CHECK( TryDropCall( f.GetScene(), a, f.At( "/b" ), DropPlace::Into ) );
}

// The panel itself in ImGui, with no window and no GPU: the mouse and the
// keys given as ImGui's events, frame after frame, and what came of them read
// from the document.
namespace
{
constexpr string_view cProject =
    R"({ "version": 1, "name": "ui", "start_level": { "path": "main.scene" }, "actions": {} })";

constexpr string_view cLevel = R"({
  "version": 1,
  "root": { "children": [ { "name": "a", "children": [ { "name": "one" } ] }, { "name": "b" }, { "name": "c" } ] }
})";

constexpr string_view cDoor = R"({
  "version": 1,
  "next_id": 2,
  "root": { "id": 0, "components": { "transform": {} }, "children": [ { "id": 1, "name": "leaf" } ] }
})";

struct Ui
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "door.prefab", string( cDoor ) },
                                 { "stone.mat", R"({ "version": 1, "params": {} })" } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    ImGuiContextScope mContext;
    Scope<ScenePanel> mPanel;
    // Where the rows began the last frame, and how far apart they are.
    ImVec2 mFirstRow{ 0, 0 };
    f32 mRowHeight = 0;

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
        mPanel = CreateScope<ScenePanel>( *mProject );
        Editor::SetUpImGui( 1.0f );
        // Textures asked for and never made: nothing is drawn.
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 400, 400 );
        Frame();
    }

    Document& GetDocument() { return *mProject->GetWorkspace().TryActive(); }
    const Scene& GetScene() { return GetDocument().GetScene(); }

    void Frame()
    {
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 400, 400 ) );
        ImGui::Begin( "Scene" );
        mFirstRow = ImGui::GetCurrentWindow()->DC.CursorStartPos;
        mRowHeight = ImGui::GetTextLineHeightWithSpacing();
        mPanel->Show();
        ImGui::End();
        ImGui::Render();
    }

    // The middle of a row's name, the rows counted from 0 as shown.
    ImVec2 Row( u32 at ) const
    {
        return ImVec2( mFirstRow.x + 40, mFirstRow.y + mRowHeight * static_cast<f32>( at ) + mRowHeight * 0.4f );
    }

    void MoveTo( ImVec2 at )
    {
        ImGui::GetIO().AddMousePosEvent( at.x, at.y );
        Frame();
    }

    void Click( u32 row, ImGuiKey modifier = ImGuiKey_None )
    {
        if ( modifier != ImGuiKey_None )
            ImGui::GetIO().AddKeyEvent( modifier, true );
        MoveTo( Row( row ) );
        ImGui::GetIO().AddMouseButtonEvent( 0, true );
        Frame();
        ImGui::GetIO().AddMouseButtonEvent( 0, false );
        Frame();
        if ( modifier != ImGuiKey_None )
        {
            ImGui::GetIO().AddKeyEvent( modifier, false );
            Frame();
        }
    }

    void Press( ImGuiKey key )
    {
        ImGui::GetIO().AddKeyEvent( key, true );
        Frame();
        ImGui::GetIO().AddKeyEvent( key, false );
        Frame();
    }

    vector<string> Selected()
    {
        vector<string> paths;
        for ( const Entity entity : GetDocument().GetSelection().All() )
            paths.push_back( PathOf( GetScene(), entity ).mPath.String() );
        return paths;
    }

    vector<string> Top()
    {
        vector<string> names;
        for ( const Entity child : ChildrenOf( GetScene(), GetScene().Root() ) )
            names.push_back( NameOf( GetScene(), child ).String() );
        return names;
    }
};
}

TEST_CASE( "The panel selects by clicks, ctrl adding; a click on one of several picks it when the button comes up" )
{
    Ui ui;
    // Rows: a (closed), b, c.
    ui.Click( 1 );
    CHECK( ui.Selected() == vector<string>{ "/b" } );
    ui.Click( 2, ImGuiMod_Ctrl );
    CHECK( ui.Selected() == vector<string>{ "/b", "/c" } );
    ui.Click( 1 );
    CHECK( ui.Selected() == vector<string>{ "/b" } );
    // Below the rows: nothing.
    ui.MoveTo( ImVec2( 100, 300 ) );
    ImGui::GetIO().AddMouseButtonEvent( 0, true );
    ui.Frame();
    ImGui::GetIO().AddMouseButtonEvent( 0, false );
    ui.Frame();
    CHECK( ui.Selected().empty() );
}

TEST_CASE( "Beside a data document's window the panel shows the scene, a click making it the active one again" )
{
    Ui ui;
    REQUIRE( ui.mProject->Run( "document.open", *Json::TryParse( R"({ "path": "stone.mat" })" ) ) );
    Workspace& workspace = ui.mProject->GetWorkspace();
    REQUIRE( workspace.TryActiveData() );
    ui.Frame();
    ui.Click( 1 );
    REQUIRE( workspace.TryActive() );
    CHECK( not workspace.TryActiveData() );
    CHECK( ui.Selected() == vector<string>{ "/b" } );
}

TEST_CASE( "The panel renames in place with F2, deletes with Del, and both undo as one step each" )
{
    Ui ui;
    ui.Click( 1 );
    ui.Press( ImGuiKey_F2 );
    ui.Frame();
    ImGui::GetIO().AddInputCharactersUTF8( "мост" );
    ui.Frame();
    ui.Press( ImGuiKey_Enter );
    CHECK( ui.Top() == vector<string>{ "a", "мост", "c" } );

    ui.Click( 2 );
    ui.Press( ImGuiKey_Delete );
    CHECK( ui.Top() == vector<string>{ "a", "мост" } );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    REQUIRE( ui.mProject->Run( "edit.undo" ) );
    CHECK( ui.Top() == vector<string>{ "a", "b", "c" } );
}

TEST_CASE( "The panel moves what is dragged onto a row into it" )
{
    Ui ui;
    // c dragged onto a: a step at a time, past ImGui's threshold.
    ui.MoveTo( ui.Row( 2 ) );
    ImGui::GetIO().AddMouseButtonEvent( 0, true );
    ui.Frame();
    const ImVec2 from = ui.Row( 2 );
    const ImVec2 to = ui.Row( 0 );
    for ( int step = 1; step <= 8; ++step )
    {
        const f32 share = static_cast<f32>( step ) / 8;
        ui.MoveTo( ImVec2( from.x, from.y + ( to.y - from.y ) * share ) );
    }
    ImGui::GetIO().AddMouseButtonEvent( 0, false );
    ui.Frame();
    ui.Frame();
    CHECK( ui.Top() == vector<string>{ "a", "b" } );
    CHECK( PathOf( ui.GetScene(), *TryResolve( ui.GetScene(), EntityPath{ Make( "/a/c" ) } ) ).mPath.String() ==
           "/a/c" );
}

TEST_CASE( "An entity made active elsewhere opens the rows over it" )
{
    Ui ui;
    REQUIRE( ui.mProject->Run( "selection.set", *Json::TryParse( R"({ "entities": [ "/a/one" ] })" ) ) );
    ui.Frame();
    // Rows now: a, one, b, c.
    ui.Click( 2 );
    CHECK( ui.Selected() == vector<string>{ "/b" } );
}

TEST_CASE( "A prefab's root is a row of its own, named by its file, its nodes under it; it is not renamed" )
{
    Ui ui;
    REQUIRE( ui.mProject->Run( "document.open", *Json::TryParse( R"({ "path": "door.prefab" })" ) ) );
    ui.Frame();
    // Rows: door (open), leaf.
    ui.Click( 1 );
    CHECK( ui.Selected() == vector<string>{ "/leaf" } );
    ui.Click( 0 );
    CHECK( ui.Selected() == vector<string>{ "/" } );
    ui.Press( ImGuiKey_F2 );
    ImGui::GetIO().AddInputCharactersUTF8( "gate" );
    ui.Frame();
    ui.Press( ImGuiKey_Enter );
    CHECK( ui.Top() == vector<string>{ "leaf" } );
    CHECK_FALSE( ui.GetDocument().IsDirty() );
    CHECK( RootLabel( ui.GetDocument() ) == "door" );
}
