#pragma once
#include "engine/engine.hpp"
#include "engine/window/window.hpp"
#include "engine/project/project.hpp"
#include "engine/physics/physics_engine.hpp"
#include "editor_user_interface/editor_user_interface.hpp"
#include "utils/resources_hot_reloader.hpp"
#include "engine/editing/selection.hpp"
#include "utils/ui_globals.hpp"
#include "utils/editor_settings.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/editing/operators/operator_queue.hpp"
#include "engine/editing/scripting/editor_lua.hpp"
#include <nlohmann/json.hpp>
#include "engine/editing/clipboard.hpp"
#include "utils/auto_backup.hpp"

namespace bubble
{
enum class EditorMode
{
    Editing,
    Running
};

// Game engine editor 
class BubbleEditor
{
public:
    BubbleEditor();
    ~BubbleEditor();
    // What an operator runs against: the editor's document and its state.
    OperatorContext Operators() { return OperatorContext{ mProject, mHistory, mSelection, mClipboard }; }
    // Run one now, logging a failure instead of throwing. For hotkeys.
    bool Invoke( const char* op, const json& args );
    bool Invoke( const char* op ) { return Invoke( op, json::object() ); }
    // An editor script (see editor_lua.hpp); the --script option.
    void RunScript( const path& file );

    void OpenProject( const path& projectPath );
    // Replace the level being edited. Relative to the project root.
    void OpenLevel( const path& relFile );
    // Save the level being edited and replace it with a new, empty one.
    void NewLevel( const string& name );
    void Run();

private:
    void OnUpdate();
    void OnUpdateHotKeys();
    void RegisterEditorOperators();

    void StartEngine();
    void StopEngine();
    void Validation();

public:
    Timer mTimer;
    Window mWindow;
    Engine mEngine;

    /// Editor
    EditorMode mEditorMode;
    SceneCamera mSceneCamera;
    Framebuffer mSceneViewport; // Viewport
    Framebuffer mEntityIdViewport; // Entity picking viewport (Handles scene object picking in viewport)
    UIGlobals mUIGlobals;
    EditorSettings mEditorSettings;

    /// Game editing
    Project mProject;
    Selection mSelection;
    History mHistory;
    Clipboard mClipboard;
    OperatorQueue mOperatorQueue;
    EditorLua mEditorLua; // after everything its context refers to
    AutoBackup mAutoBackup;
    ProjectResourcesHotReloader mProjectResourcesHotReloader;

    // Editor windows
    EditorUserInterface mEditorUserInterface;
};

}