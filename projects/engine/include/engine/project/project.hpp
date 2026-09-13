#pragma once
#include "engine/utils/filesystem.hpp"
#include "engine/scene/scene.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/any.hpp"
#include "project_tree.hpp"
#include "level.hpp"

namespace bubble
{
constexpr string_view PROJECT_LEVELS_SUBDIR = "levels"sv;

// The game: the resources it uses, the Lua VM its scripts run in, the state
// they share, and the level files under levels/. Exactly one level is open at
// a time (mLevel); switching loads into that same object.
//
// Level files are addressed by path relative to the project root everywhere -
// in the project file, in the editor and from Lua - so a project can be moved.
class Project
{
private:
    void LoadDefaultResources();

public:
    explicit Project();
    ~Project();

    // Makes <rootDir>/<projectName>/ with the project file and a first level,
    // and leaves that level open.
    void Create( const path& rootDir, const string& projectName );
    // Opens the project and, unless told not to, its startup level - the
    // engine loads a level of its own choosing right after and skips it. A
    // project file from before levels existed carries its scene inline; it is
    // split out into levels/main.level and rewritten in the new layout on the
    // spot.
    void Open( const path& rootFile, bool openStartupLevel = true );
    // Project file and the open level.
    void Save() const;
    // Same, to other paths. For backups, which must not touch mRootFile.
    void SaveTo( const path& projectFile, const path& levelFile ) const;
    bool IsValid() const;

    /// Levels
    path RootDir() const { return mRootFile.parent_path(); }
    path LevelsDir() const { return RootDir() / PROJECT_LEVELS_SUBDIR; }
    // Relative paths of every level file under levels/, sorted. Scanned, not
    // stored: a file dropped into the directory is a level.
    vector<path> Levels() const;
    // Writes an empty levels/<name>.level and opens it.
    void NewLevel( const string& name );
    // Replaces the open level. Does not save the one being left.
    void OpenLevel( const path& relFile );
    // The open level as a path relative to the project root.
    path CurrentLevel() const;

    string mName;
    path mRootFile;
    Loader mLoader;
    ScriptingEngine mScriptingEngine;
    Scope<Any> mGlobalState;
    path mStartupLevel; // relative; the level a run begins in
    Level mLevel;       // the open level
};

}
