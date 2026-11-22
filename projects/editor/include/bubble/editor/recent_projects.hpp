#pragma once
#include "bubble/types/containers.hpp"
#include "bubble/types/filesystem.hpp"
#include "bubble/types/number.hpp"
#include "bubble/types/string.hpp"
#include "bubble/types/utility.hpp"

namespace bubble
{
// A project the editor opened, as the start window lists it.
struct RecentProject
{
    // The project's folder, UTF-8.
    string mPath;
    // project.bubble's name when it was last opened.
    string mName;
    // When it was last opened: seconds since 1970, UTC.
    i64 mOpened = 0;

    bool operator==( const RecentProject& ) const = default;
};

// The projects one person opened, the last first: recent.json in the
// editor's folder beside editor.json - what the editor remembers by itself,
// not a preference -, written each time it changes.
class RecentProjects
{
public:
    // Read from `fileMaybe` when it is there: a broken file is logged and
    // the list starts empty. None: kept in memory alone, as tests do.
    explicit RecentProjects( opt<OsPath> fileMaybe );

    span<const RecentProject> All() const { return mProjects; }
    // Opened now: first in the list, once, the oldest past cMost let go.
    void Opened( const OsPath& root, string name, i64 now );
    void Forget( string_view path );

    expected<vector<RecentProject>, string> TryParse( string_view text ) const;
    string ToJson() const;

    static constexpr string_view cFileName = "recent.json";
    static constexpr size_t cMost = 16;

private:
    void Save() const;

    vector<RecentProject> mProjects;
    opt<OsPath> mFileMaybe;
};

// A new project: `name`'s folder under `parent`, with a project.bubble that
// starts on levels/main.scene - a sun and a camera -, and a .gitignore for
// the import cache. Refuses a name that is no folder's and a folder that is
// there with something in it. Returns the project's folder.
expected<OsPath, string> TryCreateProject( const OsPath& parent, string_view name );
}
