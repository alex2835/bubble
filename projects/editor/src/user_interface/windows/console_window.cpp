#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/console_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include <imgui.h>

namespace bubble
{
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

namespace
{
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

void ConsoleWindow::OnDraw( DeltaTime )
{
    if ( not mUIGlobals.mShow.mConsole )
        return;
    ImGui::Begin( Name().data(), &mUIGlobals.mShow.mConsole );

    if ( ImGui::SmallButton( "Clear" ) )
        mLua.ClearLog();
    ImGui::SameLine();
    ImGui::TextDisabled( "editor.ops.<group>.<verb>{ ... }   editor.operators()   editor.tree()" );

    const float inputHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild( "log", ImVec2( 0, -inputHeight ), true );
    for ( const auto& line : mLua.Log() )
    {
        const bool isError = line.starts_with( "error:" );
        if ( isError )
            ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ) );
        ImGui::TextWrapped( "%s", line.c_str() );
        if ( isError )
            ImGui::PopStyleColor();
    }
    if ( mScrollToBottom )
    {
        ImGui::SetScrollHereY( 1.0f );
        mScrollToBottom = false;
    }
    ImGui::EndChild();

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
    if ( ImGui::InputText( "##input", buffer, sizeof( buffer ), flags, RecallCallback, &userData ) )
    {
        const string line = buffer;
        if ( not line.empty() )
        {
            mEntered.push_back( line );
            mRecall = -1;
            mLua.Print( "> " + line );
            mLua.Run( line );
            mScrollToBottom = true;
        }
        mInput.clear();
        mFocusInput = true;
    }
    else
        mInput = buffer;

    ImGui::End();
}

}
