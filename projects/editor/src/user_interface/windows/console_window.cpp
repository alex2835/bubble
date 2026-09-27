#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/console_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include "engine/utils/imgui_utils.hpp"
#include <imgui.h>

namespace bubble
{
namespace
{
// A session's worth; the oldest rows go first.
constexpr size_t cMaxRows = 50000;

ImVec4 LevelColor( LogLevel level )
{
    switch ( level )
    {
        case LogLevel::Warning: return ImVec4( 1.0f, 0.8f, 0.35f, 1.0f );
        case LogLevel::Error: return ImVec4( 1.0f, 0.4f, 0.4f, 1.0f );
        case LogLevel::Script: return ImVec4( 0.65f, 0.85f, 1.0f, 1.0f );
        default: return ImGui::GetStyleColorVec4( ImGuiCol_Text );
    }
}

bool ContainsNoCase( string_view text, string_view part )
{
    if ( part.empty() )
        return true;
    const auto it = std::search( text.begin(), text.end(), part.begin(), part.end(),
                                 []( char a, char b ) { return std::tolower( (unsigned char)a ) == std::tolower( (unsigned char)b ); } );
    return it != text.end();
}

// Up / Down through what was entered before, the usual shell way.
int RecallCallback( ImGuiInputTextCallbackData* data )
{
    auto* self = static_cast<std::pair<vector<string>*, int*>*>( data->UserData );
    auto& entered = *self->first;
    int& recall = *self->second;
    if ( data->EventFlag != ImGuiInputTextFlags_CallbackHistory or entered.empty() )
        return 0;

    if ( data->EventKey == ImGuiKey_UpArrow )
        recall = recall < 0 ? (int)entered.size() - 1 : std::max( 0, recall - 1 );
    else if ( data->EventKey == ImGuiKey_DownArrow )
        recall = recall < 0 ? -1 : std::min( (int)entered.size(), recall + 1 );

    const string& line = recall >= 0 and recall < (int)entered.size() ? entered[recall] : string();
    data->DeleteChars( 0, data->BufTextLen );
    data->InsertChars( 0, line.c_str() );
    return 0;
}
}

ConsoleWindow::ConsoleWindow( BubbleEditor& editor )
    : UserInterfaceWindowBase( editor ),
      mLua( editor.mEditorLua )
{
}

string_view ConsoleWindow::Name()
{
    return "Console"sv;
}

void ConsoleWindow::OnUpdate( DeltaTime )
{
}

void ConsoleWindow::Pull()
{
    vector<LogEntry> fresh;
    mNextIndex = LogReadSince( mNextIndex, fresh );
    for ( auto& entry : fresh )
    {
        size_t begin = 0;
        while ( true )
        {
            const size_t end = entry.mText.find( '\n', begin );
            mRows.push_back( Row{ entry.mLevel, entry.mText.substr( begin, end - begin ) } );
            if ( end == string::npos )
                break;
            begin = end + 1;
        }
    }
    if ( mRows.size() > cMaxRows )
        mRows.erase( mRows.begin(), mRows.begin() + ( mRows.size() - cMaxRows ) );
}

bool ConsoleWindow::Shown( const Row& row ) const
{
    return mShowLevel[(int)row.mLevel] and ContainsNoCase( row.mText, mFilter );
}

void ConsoleWindow::DrawToolbar()
{
    if ( ImGui::SmallButton( "Clear" ) )
    {
        mRows.clear();
        mFollow = true;
    }
    ImGui::SameLine();
    if ( ImGui::SmallButton( "Copy" ) )
    {
        string text;
        for ( const Row& row : mRows )
            if ( Shown( row ) )
                text += row.mText + '\n';
        ImGui::SetClipboardText( text.c_str() );
    }
    static constexpr const char* cLevels[] = { "Info", "Warnings", "Errors", "Scripts" };
    for ( int i = 0; i < 4; i++ )
    {
        ImGui::SameLine();
        ImGui::PushStyleColor( ImGuiCol_Text, LevelColor( (LogLevel)i ) );
        ImGui::Checkbox( cLevels[i], &mShowLevel[i] );
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 220.0f * mWindow.GetUIScale() );
    ImGui::InputTextWithHint( "##filter", "filter", mFilter );
    ImGui::SameLine();
    ImGui::TextDisabled( "Lua below: an expression shows its value; dump( value, depth ), editor.ops..." );
}

void ConsoleWindow::DrawLog()
{
    const float inputHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild( "log", ImVec2( 0, -inputHeight ), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );

    // Follows the end until the reader scrolls away from it, and again once
    // they scroll back down. Only their scrolling decides: the list growing,
    // or the window being laid out in its first frames, does not.
    const ImGuiIO& io = ImGui::GetIO();
    if ( ImGui::IsWindowHovered() and ( io.MouseWheel != 0.0f or ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) )
        mFollow = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    vector<const Row*> shown;
    shown.reserve( mRows.size() );
    for ( const Row& row : mRows )
        if ( Shown( row ) )
            shown.push_back( &row );

    ImGuiListClipper clipper;
    clipper.Begin( (int)shown.size() );
    while ( clipper.Step() )
        for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
        {
            ImGui::PushStyleColor( ImGuiCol_Text, LevelColor( shown[i]->mLevel ) );
            ImGui::TextUnformatted( shown[i]->mText.c_str() );
            ImGui::PopStyleColor();
        }

    // The clipper leaves the cursor at the end of the whole list but
    // SetScrollHereY measures from the last item drawn - an empty one here
    // is what makes "here" the end.
    ImGui::Dummy( ImVec2( 0, 0 ) );
    if ( mFollow )
        ImGui::SetScrollHereY( 1.0f );
    ImGui::EndChild();
}

void ConsoleWindow::DrawInput()
{
    ImGui::SetNextItemWidth( -1 );
    if ( mFocusInput )
    {
        ImGui::SetKeyboardFocusHere();
        mFocusInput = false;
    }
    char buffer[1024] = { 0 };
    mInput.copy( buffer, sizeof( buffer ) - 1 );
    std::pair<vector<string>*, int*> userData{ &mEntered, &mRecall };
    const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
    if ( ImGui::InputTextWithHint( "##input", "Lua", buffer, sizeof( buffer ), flags, RecallCallback, &userData ) )
    {
        const string line = buffer;
        if ( not line.empty() )
        {
            mEntered.push_back( line );
            mRecall = -1;
            mLua.Print( "> " + line );
            mLua.RunInteractive( line );
            mFollow = true;
        }
        mInput.clear();
        mFocusInput = true;
    }
    else
        mInput = buffer;
}

void ConsoleWindow::OnDraw( DeltaTime )
{
    // Pulled even while hidden, so nothing is missed.
    Pull();
    if ( not mUIGlobals.mShow.mConsole )
        return;
    ImGui::Begin( Name().data(), &mUIGlobals.mShow.mConsole );
    DrawToolbar();
    DrawLog();
    DrawInput();
    ImGui::End();
}

}
