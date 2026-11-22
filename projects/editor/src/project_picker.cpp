#include "bubble/editor/project_picker.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/editor/editor_icons.hpp"
#include "bubble/editor/editor_style.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include "bubble/types/thread.hpp"
#include <imgui.h>
#include <imgui_stdlib.h>

namespace bubble
{
namespace
{
constexpr f32 cColumn = 680.0f;

i64 Now()
{
    return chrono::duration_cast<chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count();
}

bool IsProject( const string& path )
{
    error_code error;
    return fs::exists( PathFromUtf8( path ) / "project.bubble", error );
}
}

string AgoText( i64 then, i64 now )
{
    const i64 days = ( now - then ) / 86400;
    if ( days <= 0 )
        return "today";
    if ( days == 1 )
        return "yesterday";
    if ( days < 60 )
        return format( "{} days ago", days );
    if ( days < 730 )
        return format( "{} months ago", days / 30 );
    return format( "{} years ago", days / 365 );
}

ProjectPicker::ProjectPicker( RecentProjects& recent, ChooseFolder choose )
    : mRecent( recent ),
      mChoose( std::move( choose ) )
{
    // A new project goes beside the last one opened, at first.
    if ( not recent.All().empty() )
        mNewParent = PathToUtf8( PathFromUtf8( recent.All().front().mPath ).parent_path() );
}

void ProjectPicker::Pick( const OsPath& root )
{
    mError.clear();
    mChosenMaybe = root;
}

void ProjectPicker::Show()
{
    const ImGuiViewport& viewport = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos( viewport.WorkPos );
    ImGui::SetNextWindowSize( viewport.WorkSize );
    ImGui::Begin( "Projects", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoBringToFrontOnFocus );
    const f32 width = min( cColumn, viewport.WorkSize.x - 32.0f );
    const f32 left = ( viewport.WorkSize.x - width ) * 0.5f;
    ImGui::SetCursorPos( ImVec2( left, max( 24.0f, viewport.WorkSize.y * 0.08f ) ) );
    // A column of its own width: the list and its title end where it does.
    ImGui::BeginChild( "column", ImVec2( width, 0 ) );
    ImGui::PushItemWidth( width );

    ImGui::PushFont( nullptr, ImGui::GetFontSize() * 2.0f );
    ImGui::TextColored( EditorColor::Accent(), "Bubble" );
    ImGui::PopFont();
    ImGui::TextColored( EditorColor::cTextDim, "Open a project, or make a new one." );
    ImGui::Spacing();

    if ( ImGui::Button( format( "{}  Open Folder...", Icon::cFolder ).c_str() ) )
        mChoose( mNewParent.empty() ? OsPath() : PathFromUtf8( mNewParent ), [this]( opt<OsPath> folderMaybe ) {
            if ( folderMaybe )
                Pick( *folderMaybe );
        } );
    ImGui::SameLine();
    if ( ImGui::Button( "+  New Project..." ) )
    {
        mAskNew = true;
        mNewError.clear();
    }
    ImGui::Spacing();
    ImGui::SetNextItemWidth( width - ImGui::CalcTextSize( "Open" ).x - ImGui::GetStyle().FramePadding.x * 2 -
                             ImGui::GetStyle().ItemSpacing.x );
    const bool entered = ImGui::InputTextWithHint( "##typed", "or a project's folder: E:\\games\\my_game", &mTyped,
                                                   ImGuiInputTextFlags_EnterReturnsTrue );
    ImGui::SameLine();
    if ( ( ImGui::Button( "Open" ) or entered ) and not mTyped.empty() )
        Pick( PathFromUtf8( mTyped ) );
    if ( not mError.empty() )
    {
        ImGui::PushTextWrapPos( width );
        ImGui::TextColored( EditorColor::cError, "%s", mError.c_str() );
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    ImGui::SeparatorText( "Recent" );
    Recent();
    ImGui::PopItemWidth();
    ImGui::EndChild();
    NewProject();
    ImGui::End();
}

void ProjectPicker::Recent()
{
    const span<const RecentProject> projects = mRecent.All();
    if ( projects.empty() )
    {
        ImGui::TextColored( EditorColor::cTextDim, "None yet: the projects you open are listed here." );
        return;
    }
    const i64 now = Now();
    opt<string> forgottenMaybe;
    if ( not ImGui::BeginTable( "recent", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX ) )
        return;
    ImGui::TableSetupColumn( "project", ImGuiTableColumnFlags_WidthStretch );
    ImGui::TableSetupColumn( "when", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.5f );
    ImGui::TableSetupColumn( "forget", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight() );
    for ( const RecentProject& project : projects )
    {
        ImGui::PushID( project.mPath.c_str() );
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool there = IsProject( project.mPath );
        const string name =
            project.mName.empty() ? PathToUtf8( PathFromUtf8( project.mPath ).filename() ) : project.mName;
        // Two lines a row: the name, the folder under it.
        const f32 rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
        ImGui::BeginDisabled( not there );
        if ( ImGui::Selectable( "##open", false,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                                ImVec2( 0, rowHeight ) ) )
            Pick( PathFromUtf8( project.mPath ) );
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Open %s", project.mPath.c_str() );
        ImGui::SameLine( ImGui::GetStyle().CellPadding.x );
        ImGui::BeginGroup();
        ImGui::TextUnformatted( format( "{}  {}", Icon::cFolder, name ).c_str() );
        ImGui::TextColored( EditorColor::cTextDim, "%s%s", project.mPath.c_str(),
                            there ? "" : "  - not there any more" );
        ImGui::EndGroup();
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::TextColored( EditorColor::cTextDim, "%s", AgoText( project.mOpened, now ).c_str() );
        ImGui::TableNextColumn();
        if ( ImGui::SmallButton( "x" ) )
            forgottenMaybe = project.mPath;
        if ( ImGui::IsItemHovered() )
            ImGui::SetTooltip( "Take it off the list; the folder stays" );
        ImGui::PopID();
    }
    ImGui::EndTable();
    if ( forgottenMaybe )
        mRecent.Forget( *forgottenMaybe );
}

void ProjectPicker::NewProject()
{
    if ( mAskNew )
    {
        ImGui::OpenPopup( "New Project" );
        mAskNew = false;
    }
    ImGui::SetNextWindowSize( ImVec2( 520, 0 ) );
    if ( not ImGui::BeginPopupModal( "New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
        return;
    ImGui::TextUnformatted( "Name" );
    ImGui::SetNextItemWidth( -1 );
    if ( ImGui::IsWindowAppearing() )
        ImGui::SetKeyboardFocusHere();
    ImGui::InputText( "##name", &mNewName );
    ImGui::TextUnformatted( "In the folder" );
    ImGui::SetNextItemWidth( -ImGui::CalcTextSize( "Browse..." ).x - ImGui::GetStyle().FramePadding.x * 2 -
                             ImGui::GetStyle().ItemSpacing.x );
    ImGui::InputTextWithHint( "##parent", "E:\\games", &mNewParent );
    ImGui::SameLine();
    if ( ImGui::Button( "Browse..." ) )
        mChoose( mNewParent.empty() ? OsPath() : PathFromUtf8( mNewParent ), [this]( opt<OsPath> folderMaybe ) {
            if ( folderMaybe )
                mNewParent = PathToUtf8( *folderMaybe );
        } );
    if ( not mNewParent.empty() and not mNewName.empty() )
        ImGui::TextColored( EditorColor::cTextDim, "%s",
                            PathToUtf8( PathFromUtf8( mNewParent ) / PathFromUtf8( mNewName ) ).c_str() );
    if ( not mNewError.empty() )
    {
        ImGui::PushTextWrapPos( ImGui::GetContentRegionAvail().x );
        ImGui::TextColored( EditorColor::cError, "%s", mNewError.c_str() );
        ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();
    if ( ImGui::Button( "Create" ) or ImGui::IsKeyPressed( ImGuiKey_Enter ) )
    {
        if ( auto madeMaybe = TryCreateProject( PathFromUtf8( mNewParent ), mNewName ) )
        {
            Pick( *madeMaybe );
            ImGui::CloseCurrentPopup();
        }
        else
            mNewError = madeMaybe.error();
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel" ) or ImGui::IsKeyPressed( ImGuiKey_Escape ) )
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
}
