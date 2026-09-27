#pragma once
#include "editor_user_interface/windows/window_base.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "engine/editing/history.hpp"
#include "engine/editing/selection.hpp"
#include "engine/editing/clipboard.hpp"
#include "engine/editing/operators/operator.hpp"
#include "engine/renderer/entity_id_picker.hpp"
#include "engine/project/level.hpp"
#include <optional>

namespace bubble
{
// The prefab editor: one .prefab open as a document of its own - its own
// scene and tree, undo history, selection and clipboard - seen through its
// own camera. The tree, the inspector and the viewport are the main
// windows' own classes, pointed at this document instead of the level.
//
// The file is the document: Save writes it, and then updates the instances
// of the prefab in the open level. Nothing here touches the level before.
class PrefabEditorWindow : public UserInterfaceWindowBase
{
public:
    explicit PrefabEditorWindow( BubbleEditor& editor );
    ~PrefabEditorWindow();

    string_view Name();
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );
    // Draws the prefab's scene into this window's viewport. Before the UI,
    // like the level's.
    void Render( Engine& engine, DeltaTime dt );

    // Relative to the project root. Open replaces what is open, unsaved or not.
    void Open( const path& relFile );
    // Writes an empty prefab to the file and opens it.
    void New( const path& relFile );
    bool Save();
    void Close();

    bool HasDocument() const { return not mFile.empty(); }
    bool IsDirty() const { return mDocHistory.Version() != mSavedVersion; }
    // The prefab's operator context while this window has the focus: what
    // the editor's hotkeys then act on.
    std::optional<OperatorContext> FocusedDocument();

private:
    EditorDocument Document();
    void DrawToolbar();
    void FrameContent();

    Level mDoc;
    History mDocHistory;
    Selection mDocSelection;
    Clipboard mDocClipboard;
    SceneCamera mCamera;
    Framebuffer mColor;
    Framebuffer mIds;
    EntityIdPicker mDocPicker;
    bool mDocPendingRect = false;
    bool mDocHovered = false;
    bool mDocViewManipulating = false;

    Scope<ProjectTreeWindow> mTree;
    Scope<ProjectViewportWindow> mViewport;

    path mFile;
    u64 mSavedVersion = 0;
    bool mFocused = false;
    string mNewName;
};

}
