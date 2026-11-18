#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/editing/operators.hpp"
#include "bubble/editing/workspace.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/world/app.hpp"
#include "bubble/world/project.hpp"

namespace bubble
{
class AudioDevice;

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
    // reloads it; what went wrong is logged.
    void Refresh();
    // The level project.bubble starts the game with, as a document.
    expected<void, string> TryOpenStartLevel();

    const ProjectSettings& GetSettings() const { return mSettings; }
    App& GetApp() { return mApp; }
    AssetRegistry& GetAssets() { return mAssets; }
    const ProjectFiles& GetFiles() const { return mProject.GetFiles(); }
    Workspace& GetWorkspace() { return mWorkspace; }
    const Operators& GetOperators() const { return mOperators; }
    // Runs an operator on the workspace; its error is logged, as a menu or a
    // key that ran it has no one else to tell.
    bool Run( string_view id, const Json& args = Json() );

private:
    EditorProject( ProjectFiles files, AssetRegistry::ReadImported read, ProjectSettings settings );
    expected<void, string> TryStart( AudioDevice& audio );

    ProjectSettings mSettings;
    // Before the registry, whose loads it finishes as it goes.
    App mApp;
    ProjectAssets mProject;
    AssetRegistry mAssets;
    // Its documents hold worlds, which hold the registry's assets: after
    // both, so it goes first.
    Workspace mWorkspace;
    Operators mOperators;
};
}
