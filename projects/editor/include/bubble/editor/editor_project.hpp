#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/editing/operators.hpp"
#include "bubble/editing/workspace.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/world/app.hpp"
#include "bubble/world/project.hpp"

namespace bubble
{
class AudioDevice;
class PlaySession;

// A project open in the editor: its settings, App with every module the
// engine has - what the inspector describes and Play runs -, the registry
// over its import cache, its sources kept imported as files change, and the
// workspace of its documents with the operators that change them.
class EditorProject
{
public:
    // The project in `files`, whose import cache `read` reads: project.bubble
    // read, every source imported - what will not import logged, as the
    // editor shows it -, no document open yet. The device outlives it. A
    // project.bubble missing or wrong is an error.
    static expected<Scope<EditorProject>, string> TryOpen( ProjectFiles files,
                                                           AssetRegistry::ReadImported read,
                                                           AudioDevice& audio );
    // A project's folder on disk.
    static expected<Scope<EditorProject>, string> TryOpen( const OsPath& root, AudioDevice& audio );
    ~EditorProject();
    EditorProject( const EditorProject& ) = delete;
    EditorProject& operator=( const EditorProject& ) = delete;

    // The files looked at again: what changed is imported, and the registry
    // reloads it; what went wrong is logged. project.bubble changed - saved
    // from Project Settings, or by hand - is read again: the next Play takes
    // its actions, and scripts are checked against them anew when they are
    // others; one that does not read is logged, the settings staying.
    void Refresh();
    // The level project.bubble starts the game with, as a document.
    expected<void, string> TryOpenStartLevel();

    const ProjectSettings& GetSettings() const { return mSettings; }
    App& GetApp() { return mApp; }
    AssetRegistry& GetAssets() { return mAssets; }
    const ProjectFiles& GetFiles() const { return mProject.GetFiles(); }
    const AssetCatalog& GetCatalog() const { return mProject.GetCatalog(); }
    // Whether the source at `path` imports as `kind` - its importer's own
    // kind, or one it makes beside: a model's skeleton.
    bool Makes( const AssetPath& path, string_view kind ) const;
    // Every clip the project's models have, by name, in the order the
    // models and their clips come: what a graph's state may play.
    vector<string> ClipNames() const;
    // The importer of the source at `path`; nothing for a file no importer
    // takes.
    OptRef<const Importer> TryImporterOf( const AssetPath& path ) const;
    // What the file at `path` is: its importer's kind - "model", "script" -,
    // "level" for a level, nothing for a file no importer takes.
    string_view KindOf( const AssetPath& path ) const;
    // What the last import of `path` said was wrong with it; nothing when it
    // imported, or has not been tried.
    OptRef<const string> TryErrorOf( const AssetPath& path ) const;
    // The project's folder on disk; nothing for one in memory.
    OptRef<const OsPath> TryRoot() const;
    Workspace& GetWorkspace() { return mWorkspace; }
    const Operators& GetOperators() const { return mOperators; }
    // The game playing - play.start began it, play.stop ends it -; nothing
    // when none is.
    OptRef<PlaySession> TryPlaying();
    // Runs an operator on the workspace; its error is logged, as a menu or a
    // key that ran it has no one else to tell.
    bool Run( string_view id, const Json& args = Json() );
    bool Run( string_view id, const Value::Record& args );

private:
    EditorProject( ProjectFiles files, AssetRegistry::ReadImported read, ProjectSettings settings );
    // play.start, play.stop, play.pause, play.step: the game is the
    // project's, not the workspace's.
    void AddPlayOperators();
    expected<void, string> TryStart( AudioDevice& audio );
    // How scripts are checked with the settings' actions; the declarations
    // written under .bubble too, for a language server.
    expected<ScriptCheck, string> TryScriptCheck();
    void FollowSettings();
    // What a refresh said: logged, and kept by file.
    void Report( const ProjectRefresh& refresh );

    ProjectSettings mSettings;
    // project.bubble as it was last read.
    string mSettingsText;
    opt<OsPath> mRootMaybe;
    // By path: the error of each file that would not import last time.
    hmap<string, string> mErrors;
    // Before the registry, whose loads it finishes as it goes.
    App mApp;
    ProjectAssets mProject;
    AssetRegistry mAssets;
    // Its documents hold worlds, which hold the registry's assets: after
    // both, so it goes first.
    Workspace mWorkspace;
    Operators mOperators;
    // Last, so it goes first: its document is the workspace's.
    Scope<PlaySession> mPlay;
};
}
