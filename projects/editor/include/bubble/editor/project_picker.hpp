#pragma once
#include "bubble/editor/recent_projects.hpp"
#include "bubble/types/function.hpp"

namespace bubble
{
// The editor's start window, before a project is open: the projects opened
// before, the last first - one gone from its folder shown so, to be taken
// off the list -, a project to open - its project.bubble picked by the
// system's dialog, or typed -, and a new project made in a folder of its own.
class ProjectPicker
{
public:
    // The system's dialogs - a project's file, a folder -: from a folder,
    // answering later with what was picked, or nothing.
    using Choose = function<void( const OsPath& from, function<void( opt<OsPath> )> chosen )>;

    ProjectPicker( RecentProjects& recent, Choose chooseProject, Choose chooseFolder );

    // The whole window, between ImGui::NewFrame and ImGui::Render.
    void Show();
    // What was picked since the last call: a project.bubble, or a folder
    // typed - TryProjectRoot says which project it is.
    opt<OsPath> TakeChosen() { return std::exchange( mChosenMaybe, nullopt ); }
    // Why the project picked did not open, shown until another is picked.
    void Failed( string why ) { mError = std::move( why ); }

private:
    void Recent();
    void NewProject();
    void Pick( const OsPath& root );

    RecentProjects& mRecent;
    Choose mChooseProject;
    Choose mChooseFolder;
    opt<OsPath> mChosenMaybe;
    string mError;
    // A project's file, or its folder, typed.
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
