#pragma once
#include "bubble/editor/recent_projects.hpp"
#include "bubble/types/function.hpp"

namespace bubble
{
// The editor's start window, before a project is open: the projects opened
// before, the last first - one gone from its folder shown so, to be taken
// off the list -, a folder to open picked by the system's dialog or typed,
// and a new project made in a folder of its own.
class ProjectPicker
{
public:
    // The system's folder dialog: from a folder, answering later with the
    // folder picked, or nothing.
    using ChooseFolder = function<void( const OsPath& from, function<void( opt<OsPath> )> chosen )>;

    ProjectPicker( RecentProjects& recent, ChooseFolder choose );

    // The whole window, between ImGui::NewFrame and ImGui::Render.
    void Show();
    // The project's folder picked since the last call.
    opt<OsPath> TakeChosen() { return std::exchange( mChosenMaybe, nullopt ); }
    // Why the project picked did not open, shown until another is picked.
    void Failed( string why ) { mError = std::move( why ); }

private:
    void Recent();
    void NewProject();
    void Pick( const OsPath& root );

    RecentProjects& mRecent;
    ChooseFolder mChoose;
    opt<OsPath> mChosenMaybe;
    string mError;
    // A folder typed.
    string mTyped;
    // The new project's: its name, the folder it goes in.
    bool mAskNew = false;
    string mNewName = "my_game";
    string mNewParent;
    string mNewError;
};

// "today", "yesterday", "3 days ago", "2 months ago": how long ago `then` was
// at `now`, both in seconds since 1970.
string AgoText( i64 then, i64 now );
}
