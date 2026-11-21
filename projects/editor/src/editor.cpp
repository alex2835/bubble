#include "bubble/editor/editor.hpp"
#include "bubble/editor/editor_font.hpp"
#include "bubble/editor/graph_view.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/material_preview.hpp"
#include "bubble/editor/panel.hpp"
#include "bubble/editor/play_session.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/game.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

// After ImGui's own header, which it takes as given.
#include <ImGuizmo.h>

namespace bubble
{
namespace
{
// The log panel keeps the last of what was logged.
constexpr size_t cLogKept = 2000;
// How often the project's files are looked at, in seconds.
constexpr f64 cRefreshEvery = 0.5;

ImVec4 ColorOf( LogLevel level )
{
    switch ( level )
    {
        case LogLevel::Warning: return ImVec4( 0.95f, 0.8f, 0.35f, 1.0f );
        case LogLevel::Error: return ImVec4( 1.0f, 0.45f, 0.4f, 1.0f );
        case LogLevel::Script: return ImVec4( 0.6f, 0.85f, 1.0f, 1.0f );
        case LogLevel::Info: break;
    }
    return ImGui::GetStyleColorVec4( ImGuiCol_Text );
}
}

Editor::Editor( EditorProject& project, Gpu& gpu, Renderer& renderer, function<void( const OsPath& )> open )
    : mProject( project ),
      mGpu( gpu ),
      mRenderer( renderer ),
      mScene( project ),
      mInspector( project ),
      mAssets( project, std::move( open ) ),
      mViewport( project, gpu, renderer )
{
    // A texture's file shown small: loaded the first time it is shown, its
    // GPU copy the renderer's.
    mAssets.SetThumbnail( [this]( const AssetPath& path ) -> opt<ImTextureID> {
        auto found = mThumbnails.find( path.String() );
        if ( found == mThumbnails.end() )
        {
            const auto idMaybe = mProject.GetCatalog().TryIdOf( path );
            if ( not idMaybe )
                return nullopt;
            found = mThumbnails.emplace( path.String(), mProject.GetAssets().Load<TextureAsset>( *idMaybe ) ).first;
        }
        const auto textureMaybe = mRenderer.Assets().TryTexture( found->second );
        if ( not textureMaybe )
            return nullopt;
        return ImGuiRenderer::TextureId( textureMaybe->View() );
    } );
}

void Editor::SetUpImGui( f32 scale )
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    // The font's bytes are the program's own: the atlas keeps the pointer.
    ImFontConfig font;
    font.FontDataOwnedByAtlas = false;
    const span<const u8> bytes = EditorFont();
    io.Fonts->AddFontFromMemoryTTF( const_cast<u8*>( bytes.data() ), static_cast<int>( bytes.size() ), 0.0f, &font );
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 15.0f;
    style.ScaleAllSizes( scale );
    style.FontScaleDpi = scale;
}

namespace
{
string FileNameOf( string_view path )
{
    return string( path.substr( path.rfind( '/' ) + 1 ) );
}

bool IsMaterial( const DataDocument& document )
{
    return document.GetPath().View().ends_with( ".mat" );
}

// A data document's window by its file: where it was docked is kept with
// the layout from one time to the next.
string WindowName( const DataDocument& document )
{
    return format( "{}###asset {}", FileNameOf( document.GetPath().View() ), document.GetPath().View() );
}
}

OptRef<DocumentWorld> Editor::TryShownWorld()
{
    const auto sceneMaybe = mProject.GetWorkspace().TryShownScene();
    if ( not sceneMaybe )
        return nullopt;
    return ViewOf( mProject.GetApp(), *sceneMaybe );
}

opt<string> Editor::TryActiveTitle()
{
    Workspace& workspace = mProject.GetWorkspace();
    if ( const auto dataMaybe = workspace.TryActiveData() )
        return FileNameOf( dataMaybe->GetPath().View() );
    if ( const auto activeMaybe = workspace.TryActive() )
    {
        const auto pathMaybe = activeMaybe->TryPath();
        return pathMaybe ? FileNameOf( pathMaybe->View() ) : "untitled"s;
    }
    return nullopt;
}

void Editor::Frame( f64 elapsed )
{
    mInspector.Settle();
    ImGuizmo::BeginFrame();
    mSinceRefresh += elapsed;
    if ( mSinceRefresh >= cRefreshEvery )
    {
        mSinceRefresh = 0;
        mProject.Refresh();
    }
    // The world gathers what it shows - models, lights, the transforms
    // between them - and what the viewport draws over the selection; each
    // material's preview its sphere.
    Workspace& workspace = mProject.GetWorkspace();
    {
        // Looked up for this part alone: a key, the menu or a tab below may
        // stop the game, and its document and world go with it.
        const auto shownMaybe = TryShownWorld();
        mViewport.Prepare( workspace.TryShownScene(),
                           shownMaybe ? OptRef<World>( shownMaybe->Get() ) : OptRef<World>() );
        const auto playMaybe = mProject.TryPlaying();
        if ( shownMaybe )
        {
            // A game shown has its frame below, not an editing world's. Its
            // UI laid out for the picture, as last shown.
            if ( not playMaybe or &playMaybe->View() != &*shownMaybe )
            {
                const vec2 size = glm::max( mViewport.Size(), vec2( 1.0f ) );
                shownMaybe->Get().Get<UiWorld>().SetScreen(
                    ivec2( static_cast<i32>( size.x ), static_cast<i32>( size.y ) ) );
                shownMaybe->Get().Frame( elapsed );
            }
            shownMaybe->Gathered();
        }
        if ( playMaybe )
        {
            playMaybe->Frame( elapsed );
            // Begun: the game has the input at once.
            if ( not mWasPlaying )
                mReleased = false;
        }
        mWasPlaying = static_cast<bool>( playMaybe );
    }
    // A click anywhere but on the game takes its input back; on it, gives it
    // again (below, in the viewport).
    if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) or ImGui::IsMouseClicked( ImGuiMouseButton_Right ) or
         ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) )
        mReleased = true;
    for ( const Scope<DataDocument>& document : workspace.AllData() )
        if ( IsMaterial( *document ) )
        {
            MaterialPreview& preview = PreviewOf( mProject, *document );
            preview.Refresh( *document );
            preview.Get().Frame( elapsed );
        }
        else if ( GraphView& view = GraphViewOf( mProject, *document ); view.PreviewShown() )
            view.Preview( *document ).Frame( *document, elapsed );

    Shortcuts();
    const ImGuiID dockSpace = ImHashStr( "bubble editor" );
    Layout( dockSpace );
    DockAssetWindows();
    Menu();
    ImGui::DockSpaceOverViewport( dockSpace, ImGui::GetMainViewport() );

    if ( ImGui::Begin( "Scene" ) )
        mScene.Show();
    ImGui::End();
    if ( ImGui::Begin( "Inspector" ) )
        mInspector.Show();
    ImGui::End();
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
    const bool viewport = ImGui::Begin( "Viewport" );
    ImGui::PopStyleVar();
    if ( viewport )
    {
        DocumentTabs();
        // As they are now: Stop - Ctrl+P, the menu, the game's tab closed -
        // may have taken the game's document and world away since the top.
        const auto shownMaybe = TryShownWorld();
        const auto playMaybe = mProject.TryPlaying();
        if ( shownMaybe )
        {
            if ( playMaybe and ClickedInWindow() )
                mReleased = false;
            mViewport.Show( workspace.TryShownScene(), shownMaybe->Camera() );
            if ( playMaybe and &playMaybe->View() == &*shownMaybe )
            {
                playMaybe->SetScreen( mViewport.Corner(), mViewport.Size() );
                // Below, out of the way of the game's own UI: how the input
                // goes to the game and comes back - a held cursor shows no
                // way out of itself.
                if ( mViewport.ShowsGame() )
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2( mViewport.Corner().x + 8,
                                mViewport.Corner().y + mViewport.Size().y - ImGui::GetFontSize() - 6 ),
                        IM_COL32( 255, 255, 255, mGameInput ? 140 : 220 ),
                        mGameInput ? "Shift+F1 takes the mouse and the keys back from the game; Ctrl+P stops it"
                                   : "Click to give the game the mouse and the keys; Shift+F1 takes them back" );
            }
        }
        else
            ImGui::TextDisabled( "No level or prefab is open" );
    }
    ImGui::End();
    AssetWindows();
    LogPanel();
    // After the log: a window made last is the tab shown, the first time.
    if ( ImGui::Begin( "Assets" ) )
        mAssets.Show();
    ImGui::End();
    // A file picked there is the inspector's next frame.
    if ( auto pickedMaybe = mAssets.TakePicked() )
        mInspector.Inspect( std::move( *pickedMaybe ) );
    UnsavedChanges();
    SaveAs();
    QuitDialog();
    GameInput();
    // A game that asked to quit is stopped, between frames.
    if ( const auto quitMaybe = mProject.TryPlaying(); quitMaybe and quitMaybe->GetWorld().Get<Game>().QuitAsked() )
        mProject.Run( "play.stop" );
    const auto dataMaybe = workspace.TryActiveData();
    const auto activeMaybe = workspace.TryActive();
    // A window held back is brought forward when it first shows.
    if ( dataMaybe )
        mLastActive = mHeld.contains( WindowName( *dataMaybe ) ) ? nullptr : static_cast<const void*>( &*dataMaybe );
    else
        mLastActive = activeMaybe ? static_cast<const void*>( &*activeMaybe ) : nullptr;
}

void Editor::Draw()
{
    if ( const auto shownMaybe = TryShownWorld() )
        mViewport.Draw( shownMaybe->Get(), shownMaybe->Camera() );
    for ( DataDocument* document : mPictured )
    {
        if ( IsMaterial( *document ) )
        {
            MaterialPreview& preview = PreviewOf( mProject, *document );
            mPictures.at( document )->Draw( preview.Get(), preview.Camera() );
        }
        else if ( const auto previewMaybe = GraphViewOf( mProject, *document ).TryPreview() )
            mPictures.at( document )->Draw( previewMaybe->Get(), previewMaybe->Camera() );
    }
}

void Editor::Layout( u32 dockSpace )
{
    if ( mLaidOut )
        return;
    mLaidOut = true;
    // A layout from the last time stands; the first time, this one.
    const ImGuiDockNode* node = ImGui::DockBuilderGetNode( dockSpace );
    if ( node != nullptr and not node->IsLeafNode() )
        return;
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode( dockSpace );
    ImGui::DockBuilderAddNode( dockSpace, ImGuiDockNodeFlags_DockSpace );
    ImGui::DockBuilderSetNodeSize( dockSpace, viewport.WorkSize );
    ImGuiID centre = dockSpace;
    const ImGuiID bottom = ImGui::DockBuilderSplitNode( centre, ImGuiDir_Down, 0.22f, nullptr, &centre );
    const ImGuiID left = ImGui::DockBuilderSplitNode( centre, ImGuiDir_Left, 0.2f, nullptr, &centre );
    const ImGuiID right = ImGui::DockBuilderSplitNode( centre, ImGuiDir_Right, 0.28f, nullptr, &centre );
    ImGui::DockBuilderDockWindow( "Scene", left );
    ImGui::DockBuilderDockWindow( "Inspector", right );
    ImGui::DockBuilderDockWindow( "Viewport", centre );
    ImGui::DockBuilderDockWindow( "Assets", bottom );
    ImGui::DockBuilderDockWindow( "Log", bottom );
    ImGui::DockBuilderFinish( dockSpace );
}

void Editor::MenuItem( const char* label, const char* keys, string_view op )
{
    const auto pollMaybe = mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op );
    if ( ImGui::MenuItem( label, keys, false, pollMaybe.has_value() ) )
        mProject.Run( op );
    if ( not pollMaybe and ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
        ImGui::SetTooltip( "%s", pollMaybe.error().c_str() );
}

void Editor::Menu()
{
    if ( not ImGui::BeginMainMenuBar() )
        return;
    if ( ImGui::BeginMenu( "File" ) )
    {
        MenuItem( "New Level", nullptr, "document.new" );
        if ( ImGui::MenuItem( "Save", "Ctrl+S", false, TryActiveTitle().has_value() ) )
            Save();
        if ( ImGui::MenuItem( "Save As...", nullptr, false, TryActiveTitle().has_value() ) )
            mAskSaveAs = true;
        ImGui::Separator();
        if ( ImGui::MenuItem( "Quit" ) )
            mQuitWanted = true;
        ImGui::EndMenu();
    }
    if ( ImGui::BeginMenu( "Edit" ) )
    {
        MenuItem( "Undo", "Ctrl+Z", "edit.undo" );
        MenuItem( "Redo", "Ctrl+Y", "edit.redo" );
        ImGui::EndMenu();
    }
    PlayButtons();
    ImGui::EndMainMenuBar();
}

void Editor::PlayButtons()
{
    ImGui::Separator();
    const auto playMaybe = mProject.TryPlaying();
    const auto button = [&]( const char* label, string_view op, const char* tip ) {
        const auto pollMaybe = mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op );
        if ( ImGui::MenuItem( label, nullptr, false, pollMaybe.has_value() ) )
            mProject.Run( op );
        if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
            ImGui::SetTooltip( "%s", pollMaybe ? tip : pollMaybe.error().c_str() );
    };
    if ( playMaybe )
        button( "Stop", "play.stop", "Ctrl+P: the game's copy thrown away" );
    else
        button( "Play", "play.start", "Ctrl+P: the scene shown, played on a copy" );
    const bool paused = playMaybe and playMaybe->GetWorld().Paused();
    button( paused ? "Resume" : "Pause", "play.pause", "Ctrl+Shift+P" );
    button( "Step", "play.step", "One fixed step of the game paused" );
}

void Editor::GameInput()
{
    const auto playMaybe = mProject.TryPlaying();
    const auto shownMaybe = mProject.GetWorkspace().TryShownScene();
    const bool gameInput =
        playMaybe and shownMaybe and &*shownMaybe == &playMaybe->Played() and mViewport.ShowsGame() and not mReleased;
    // Turned away, the game lets go of what was held: nothing stays down.
    if ( mGameInput and not gameInput and playMaybe )
        playMaybe->Release();
    mGameInput = gameInput;
}

void Editor::Handle( const InputEvent& event )
{
    if ( not mGameInput )
        return;
    if ( const auto playMaybe = mProject.TryPlaying() )
        playMaybe->Handle( event );
}

bool Editor::CursorLocked()
{
    const auto playMaybe = mProject.TryPlaying();
    return mGameInput and playMaybe and playMaybe->GetWorld().Get<Game>().CursorLocked();
}

void Editor::Shortcuts()
{
    // Wherever the focus is, but a field that takes text keeps its own
    // Ctrl+Z, and a drag under way - one step that is not over - takes none;
    // saving works from anywhere.
    const bool typing = ImGui::GetIO().WantTextInput or ImGui::IsAnyItemActive();
    // Play and stop from anywhere, the game's input or not; the rest are the
    // editor's while the game does not have the keys.
    if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_P ) )
        mProject.Run( mProject.TryPlaying() ? "play.stop" : "play.start" );
    if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P ) and mProject.TryPlaying() )
        mProject.Run( "play.pause" );
    if ( ImGui::IsKeyChordPressed( ImGuiMod_Shift | ImGuiKey_F1 ) )
        mReleased = true;
    if ( mGameInput )
        return;
    const auto key = [&]( ImGuiKeyChord chord, string_view op, bool always = false ) {
        if ( ( always or not typing ) and ImGui::IsKeyChordPressed( chord ) and
             mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op ) )
            mProject.Run( op );
    };
    if ( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_S ) and TryActiveTitle() )
        Save();
    key( ImGuiMod_Ctrl | ImGuiKey_Z, "edit.undo" );
    key( ImGuiMod_Ctrl | ImGuiKey_C, "edit.copy" );
    key( ImGuiMod_Ctrl | ImGuiKey_X, "edit.cut" );
    key( ImGuiMod_Ctrl | ImGuiKey_V, "edit.paste" );
    key( ImGuiMod_Ctrl | ImGuiKey_D, "entity.duplicate" );
    key( ImGuiMod_Ctrl | ImGuiKey_G, "entity.group" );
    key( ImGuiMod_Ctrl | ImGuiKey_Y, "edit.redo" );
    key( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, "edit.redo" );
}

namespace
{
// A document's tab by the document, whatever its title says.
string TabId( const void* document )
{
    return format( "###document{}", document );
}

// A level's or a prefab's tab as the bar shows it.
struct Tab
{
    string mId;
    string mTitle;
    bool mDirty = false;
    Document& mDocument;
};

}

void Editor::DocumentTabs()
{
    Workspace& workspace = mProject.GetWorkspace();
    const auto shownMaybe = workspace.TryShownScene();
    const string activeTab = shownMaybe ? TabId( &*shownMaybe ) : string();
    const bool changed = activeTab != mActiveTab;
    mActiveTab = activeTab;
    vector<Tab> tabs;
    const auto playMaybe = mProject.TryPlaying();
    for ( const Scope<Document>& document : workspace.All() )
    {
        const auto pathMaybe = document->TryPath();
        const bool played = playMaybe and &playMaybe->Played() == document.get();
        tabs.push_back( Tab{ .mId = TabId( document.get() ),
                             .mTitle = played      ? format( "{} (playing)", playMaybe->Title() )
                                       : pathMaybe ? FileNameOf( pathMaybe->View() )
                                                   : "untitled"s,
                             .mDirty = document->IsDirty() and not played,
                             .mDocument = *document } );
    }
    if ( not ImGui::BeginTabBar( "documents", ImGuiTabBarFlags_Reorderable ) )
        return;
    // What the tabs ask is done after them: both change the order of the
    // workspace's documents.
    opt<size_t> picked;
    opt<size_t> closed;
    for ( size_t at = 0; at < tabs.size(); ++at )
    {
        const Tab& tab = tabs[at];
        ImGuiTabItemFlags flags = tab.mDirty ? ImGuiTabItemFlags_UnsavedDocument : 0;
        if ( changed and tab.mId == activeTab )
            flags |= ImGuiTabItemFlags_SetSelected;
        bool open = true;
        if ( ImGui::BeginTabItem( ( tab.mTitle + tab.mId ).c_str(), &open, flags ) )
        {
            if ( not changed and tab.mId != activeTab )
                picked = at;
            ImGui::EndTabItem();
        }
        if ( not open )
            closed = at;
    }
    ImGui::EndTabBar();
    if ( picked )
        workspace.Activate( tabs[*picked].mDocument );
    if ( closed )
    {
        // A game's tab closed stops it.
        if ( tabs[*closed].mDocument.IsPlayed() )
            mProject.Run( "play.stop" );
        else
        {
            workspace.Activate( tabs[*closed].mDocument );
            if ( tabs[*closed].mDirty )
                mAskUnsaved = true;
            else
                mProject.Run( "document.close" );
        }
    }
}

void Editor::DockAssetWindows()
{
    mHeld.clear();
    const ImGuiWindow* viewport = ImGui::FindWindowByName( "Viewport" );
    const bool docked =
        viewport != nullptr and viewport->DockId != 0 and ImGui::DockBuilderGetNode( viewport->DockId ) != nullptr;
    const auto placeOf = []( const string& name ) -> opt<ImGuiID> {
        if ( const ImGuiWindow* window = ImGui::FindWindowByName( name.c_str() ) )
            return window->DockId;
        if ( const ImGuiWindowSettings* settings = ImGui::FindWindowSettingsByID( ImHashStr( name.c_str() ) ) )
            return settings->DockId;
        return nullopt;
    };
    ImGuiID assetNode = 0;
    for ( const Scope<DataDocument>& document : mProject.GetWorkspace().AllData() )
    {
        const string name = WindowName( *document );
        // Where the user left it: floating, or docked somewhere still there.
        if ( const auto placeMaybe = placeOf( name );
             placeMaybe and ( *placeMaybe == 0 or ImGui::DockBuilderGetNode( *placeMaybe ) != nullptr ) )
            continue;
        // The viewport not there yet - the first frame -, the window waits
        // for it; a viewport floating, the window floats too.
        if ( viewport == nullptr )
        {
            mHeld.insert( name );
            continue;
        }
        if ( not docked )
            continue;
        // With the other asset windows - shown, or docked a moment ago -;
        // the first one splits the viewport's right off for them.
        if ( assetNode == 0 )
            for ( const Scope<DataDocument>& other : mProject.GetWorkspace().AllData() )
                if ( const auto placeMaybe = placeOf( WindowName( *other ) ); other != document and placeMaybe and
                                                                              *placeMaybe != 0 and
                                                                              ImGui::DockBuilderGetNode( *placeMaybe ) )
                    assetNode = *placeMaybe;
        if ( assetNode == 0 )
        {
            // A share of the viewport's node, kept as the asset node's own
            // size - the viewport's, the dock space's middle, takes the rest.
            // The first frames lay the dock space out after this: the node's
            // size is its parent's then, the size it asked for is right.
            const ImGuiDockNode& node = *ImGui::DockBuilderGetNode( viewport->DockId );
            const ImVec2 size( node.SizeRef.x > 0 ? min( node.Size.x, node.SizeRef.x ) : node.Size.x,
                               node.SizeRef.y > 0 ? min( node.Size.y, node.SizeRef.y ) : node.Size.y );
            assetNode = ImGui::DockBuilderSplitNode( viewport->DockId, ImGuiDir_Right, 0.45f, nullptr, nullptr );
            ImGui::DockBuilderSetNodeSize( assetNode, ImVec2( size.x * 0.45f, size.y ) );
        }
        ImGui::DockBuilderDockWindow( name.c_str(), assetNode );
    }
}

void Editor::AssetWindows()
{
    Workspace& workspace = mProject.GetWorkspace();
    // Pictures of materials no longer open go.
    erase_if( mPictures, [&]( const auto& entry ) {
        return ranges::none_of( workspace.AllData(),
                                [&]( const Scope<DataDocument>& open ) { return open.get() == entry.first; } );
    } );
    mPictured.clear();
    const auto activeMaybe = workspace.TryActiveData();
    // Drawn in the order they were opened, the active one last: the list's.
    vector<DataDocument*> documents;
    for ( const Scope<DataDocument>& document : workspace.AllData() )
        documents.push_back( document.get() );
    DataDocument* closed = nullptr;
    for ( DataDocument* document : documents )
    {
        if ( mHeld.contains( WindowName( *document ) ) )
            continue;
        // Made active by an operator - opened, picked from a list -, its
        // window comes forward.
        if ( activeMaybe and &*activeMaybe == document and mLastActive != document )
            ImGui::SetNextWindowFocus();
        bool open = true;
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
        const bool shown = ImGui::Begin( WindowName( *document ).c_str(), &open,
                                         document->IsDirty() ? ImGuiWindowFlags_UnsavedDocument : 0 );
        ImGui::PopStyleVar();
        if ( shown )
        {
            if ( IsMaterial( *document ) )
            {
                if ( ClickedInWindow() )
                    workspace.Activate( *document );
                Scope<WorldPicture>& picture = mPictures[document];
                if ( not picture )
                    picture = CreateScope<WorldPicture>( mGpu, mRenderer );
                picture->Show( PreviewOf( mProject, *document ).Camera() );
                mPictured.push_back( document );
            }
            else
            {
                GraphView& view = GraphViewOf( mProject, *document );
                if ( view.PreviewShown() )
                {
                    // The canvas, and the graph played on a model beside it.
                    const ImVec2 area = ImGui::GetContentRegionAvail();
                    if ( ImGui::BeginChild( "canvas", ImVec2( area.x * 0.6f, area.y ) ) )
                        view.Show( *document );
                    ImGui::EndChild();
                    ImGui::SameLine( 0.0f, 0.0f );
                    if ( ImGui::BeginChild( "preview", ImVec2( 0.0f, area.y ) ) )
                    {
                        if ( ClickedInWindow() )
                            workspace.Activate( *document );
                        view.PreviewControls( *document );
                        Scope<WorldPicture>& picture = mPictures[document];
                        if ( not picture )
                            picture = CreateScope<WorldPicture>( mGpu, mRenderer );
                        picture->Show( view.Preview( *document ).Camera() );
                        mPictured.push_back( document );
                    }
                    ImGui::EndChild();
                }
                else
                    view.Show( *document );
            }
        }
        ImGui::End();
        if ( not open )
            closed = document;
    }
    if ( closed != nullptr )
    {
        workspace.Activate( *closed );
        if ( closed->IsDirty() )
            mAskUnsaved = true;
        else
            mProject.Run( "document.close" );
    }
}

void Editor::Save()
{
    Workspace& workspace = mProject.GetWorkspace();
    const auto activeMaybe = workspace.TryActive();
    // A data document always has its file; a level with none yet asks where;
    // a game playing is told it is not saved.
    if ( workspace.TryActiveData() or ( activeMaybe and ( activeMaybe->TryPath() or activeMaybe->IsPlayed() ) ) )
        mProject.Run( "document.save" );
    else if ( activeMaybe )
        mAskSaveAs = true;
}

void Editor::SaveAs()
{
    Workspace& workspace = mProject.GetWorkspace();
    if ( mAskSaveAs and TryActiveTitle() )
    {
        mAskSaveAs = false;
        const auto activeMaybe = workspace.TryActive();
        const auto pathMaybe = activeMaybe ? activeMaybe->TryPath() : nullopt;
        if ( const auto dataMaybe = workspace.TryActiveData() )
            mSaveAsPath = dataMaybe->GetPath().String();
        else
            mSaveAsPath = pathMaybe
                              ? pathMaybe->String()
                              : format( "{}untitled.scene", mAssets.Folder().empty() ? "" : mAssets.Folder() + "/" );
        ImGui::OpenPopup( "Save as" );
    }
    if ( not ImGui::BeginPopupModal( "Save as", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;
    ImGui::TextUnformatted( "Where in the project:" );
    if ( ImGui::IsWindowAppearing() )
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth( ImGui::GetFontSize() * 24 );
    const bool entered = ImGui::InputText( "##path", &mSaveAsPath, ImGuiInputTextFlags_EnterReturnsTrue );
    if ( ImGui::Button( "Save" ) or entered )
    {
        if ( mProject.Run( "document.save",
                           Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( mSaveAsPath ) } } ) ) )
            ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) or ImGui::IsKeyPressed( ImGuiKey_Escape ) )
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::QuitDialog()
{
    Workspace& workspace = mProject.GetWorkspace();
    // What would be lost: levels and prefabs - not a game's copy -, then
    // materials and graphs.
    const auto unsaved = [&] {
        vector<string> names;
        for ( const Scope<Document>& document : workspace.All() )
            if ( document->IsDirty() and not document->IsPlayed() )
                names.push_back( document->TryPath() ? string( document->TryPath()->View() ) : "untitled"s );
        for ( const Scope<DataDocument>& document : workspace.AllData() )
            if ( document->IsDirty() )
                names.emplace_back( document->GetPath().View() );
        return names;
    };
    if ( mQuitWanted )
    {
        mQuitWanted = false;
        if ( unsaved().empty() )
        {
            mQuitAsked = true;
            return;
        }
        ImGui::OpenPopup( "Quit" );
    }
    if ( not ImGui::BeginPopupModal( "Quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;
    const vector<string> names = unsaved();
    ImGui::TextUnformatted( "These have changes that are not saved:" );
    for ( const string& name : names )
        ImGui::BulletText( "%s", name.c_str() );
    if ( ImGui::Button( "Save All and Quit" ) )
    {
        // Each made active and saved; one that cannot be - a level with no
        // file yet - keeps the editor open, the log saying why.
        bool saved = true;
        const auto save = [&]( auto& document ) {
            if ( not document->IsDirty() )
                return;
            workspace.Activate( *document );
            saved = mProject.Run( "document.save" ) and saved;
        };
        vector<Document*> scenes;
        for ( const Scope<Document>& document : workspace.All() )
            if ( not document->IsPlayed() )
                scenes.push_back( document.get() );
        vector<DataDocument*> data;
        for ( const Scope<DataDocument>& document : workspace.AllData() )
            data.push_back( document.get() );
        for ( Document* document : scenes )
            save( document );
        for ( DataDocument* document : data )
            save( document );
        if ( saved )
        {
            mQuitAsked = true;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Quit Without Saving" ) )
    {
        mQuitAsked = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) or ImGui::IsKeyPressed( ImGuiKey_Escape ) )
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::UnsavedChanges()
{
    if ( mAskUnsaved )
    {
        mAskUnsaved = false;
        ImGui::OpenPopup( "Unsaved changes" );
    }
    if ( not ImGui::BeginPopupModal( "Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;
    const auto titleMaybe = TryActiveTitle();
    if ( not titleMaybe )
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::Text( "%s has changes that are not saved.", titleMaybe->c_str() );
    if ( ImGui::Button( "Save" ) )
    {
        if ( mProject.Run( "document.save" ) )
            mProject.Run( "document.close" );
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Don't Save" ) )
    {
        mProject.Run( "document.close",
                      Json( Json::Object{ Json::Member{ .mKey = "discard", .mValue = Json( true ) } } ) );
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) )
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::LogPanel()
{
    mLogNext = LogReadSince( mLogNext, mLog );
    if ( mLog.size() > cLogKept )
        mLog.erase( mLog.begin(), mLog.end() - cLogKept );
    if ( not ImGui::Begin( "Log" ) )
    {
        ImGui::End();
        return;
    }
    if ( ImGui::SmallButton( "Clear" ) )
        mLog.clear();
    ImGui::Separator();
    if ( ImGui::BeginChild( "lines", ImVec2( 0, 0 ), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        // Kept at the bottom while it is there, so new lines show.
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        ImGuiListClipper clipper;
        clipper.Begin( static_cast<int>( mLog.size() ) );
        while ( clipper.Step() )
            for ( int at = clipper.DisplayStart; at < clipper.DisplayEnd; ++at )
            {
                const LogEntry& entry = mLog[static_cast<size_t>( at )];
                ImGui::PushStyleColor( ImGuiCol_Text, ColorOf( entry.mLevel ) );
                ImGui::TextUnformatted( entry.mText.data(), entry.mText.data() + entry.mText.size() );
                ImGui::PopStyleColor();
            }
        if ( atBottom )
            ImGui::SetScrollHereY( 1.0f );
    }
    ImGui::EndChild();
    ImGui::End();
}

string Editor::Title()
{
    const string project = mProject.GetSettings().mName.empty() ? "bubble" : mProject.GetSettings().mName;
    if ( const auto dataMaybe = mProject.GetWorkspace().TryActiveData() )
        return format( "{}{} - {} - bubble editor", dataMaybe->GetPath().View(), dataMaybe->IsDirty() ? " *" : "",
                       project );
    const auto activeMaybe = mProject.GetWorkspace().TryActive();
    if ( not activeMaybe )
        return format( "{} - bubble editor", project );
    if ( const auto playMaybe = mProject.TryPlaying(); playMaybe and &playMaybe->Played() == &*activeMaybe )
        return format( "{} (playing) - {} - bubble editor", playMaybe->Title(), project );
    const auto pathMaybe = activeMaybe->TryPath();
    return format( "{}{} - {} - bubble editor", pathMaybe ? pathMaybe->View() : "untitled",
                   activeMaybe->IsDirty() ? " *" : "", project );
}
}
