#include "bubble/editor/editor.hpp"
#include "bubble/editor/document_world.hpp"
#include "bubble/editor/editor_font.hpp"
#include <imgui.h>
#include <imgui_internal.h>

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

Editor::Editor( EditorProject& project, Gpu& gpu, Renderer& renderer ) : mProject( project ), mViewport( gpu, renderer )
{
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

OptRef<World> Editor::TryActiveWorld()
{
    const auto activeMaybe = mProject.GetWorkspace().TryActive();
    if ( not activeMaybe )
        return nullopt;
    return WorldOf( mProject.GetApp(), *activeMaybe );
}

void Editor::Frame( f64 elapsed )
{
    mSinceRefresh += elapsed;
    if ( mSinceRefresh >= cRefreshEvery )
    {
        mSinceRefresh = 0;
        mProject.Refresh();
    }
    // The world gathers what it shows: models, lights, the transforms
    // between them.
    if ( const auto worldMaybe = TryActiveWorld() )
        worldMaybe->Frame( elapsed );

    Shortcuts();
    const ImGuiID dockSpace = ImHashStr( "bubble editor" );
    Layout( dockSpace );
    Menu();
    ImGui::DockSpaceOverViewport( dockSpace, ImGui::GetMainViewport() );

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
    if ( ImGui::Begin( "Viewport" ) )
        mViewport.Show();
    ImGui::End();
    ImGui::PopStyleVar();
    LogPanel();
}

void Editor::Draw()
{
    if ( const auto worldMaybe = TryActiveWorld() )
        mViewport.Draw( *worldMaybe );
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
    ImGui::DockBuilderDockWindow( "Viewport", centre );
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
        MenuItem( "Save", "Ctrl+S", "document.save" );
        ImGui::Separator();
        if ( ImGui::MenuItem( "Quit" ) )
            mQuitAsked = true;
        ImGui::EndMenu();
    }
    if ( ImGui::BeginMenu( "Edit" ) )
    {
        MenuItem( "Undo", "Ctrl+Z", "edit.undo" );
        MenuItem( "Redo", "Ctrl+Y", "edit.redo" );
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void Editor::Shortcuts()
{
    // Global, after whatever has the focus: a text field keeps its own
    // Ctrl+Z.
    const auto key = [&]( ImGuiKeyChord chord, string_view op ) {
        if ( ImGui::Shortcut( chord, ImGuiInputFlags_RouteGlobal ) and
             mProject.GetOperators().TryPoll( mProject.GetWorkspace(), op ) )
            mProject.Run( op );
    };
    key( ImGuiMod_Ctrl | ImGuiKey_S, "document.save" );
    key( ImGuiMod_Ctrl | ImGuiKey_Z, "edit.undo" );
    key( ImGuiMod_Ctrl | ImGuiKey_Y, "edit.redo" );
    key( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, "edit.redo" );
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
    const auto activeMaybe = mProject.GetWorkspace().TryActive();
    if ( not activeMaybe )
        return format( "{} - bubble editor", project );
    const auto pathMaybe = activeMaybe->TryPath();
    return format( "{}{} - {} - bubble editor", pathMaybe ? pathMaybe->View() : "untitled",
                   activeMaybe->IsDirty() ? " *" : "", project );
}
}
