#pragma once
#include "engine/engine.hpp"
#include "engine/scene/scene.hpp"
#include "engine/project/project_tree.hpp"
#include "utils/scene_camera.hpp"

namespace bubble
{
class BubbleEditor;
class Window;
class Selection;
class History;
class Clipboard;
class OperatorQueue;
class EntityIdPicker;
struct OperatorContext;
struct UIGlobals;
struct EditorSettings;
enum class EditorMode;

// What a scene view edits and what it looks through: a level-shaped document
// with its own history, selection and clipboard, and the camera, render
// targets and picking state of the view onto it. The main windows share the
// project's open level; the prefab editor has one of its own, and the same
// tree, inspector and viewport windows work on either.
struct EditorDocument
{
    Level& mLevel;
    History& mHistory;
    Selection& mSelection;
    Clipboard& mClipboard;
    SceneCamera& mCamera;
    Framebuffer& mSceneViewport;
    Framebuffer& mEntityIdViewport;
    EntityIdPicker& mPicker;
    bool& mPendingRectSelect;
    bool& mViewportHovered;
    bool& mViewManipulatorUsing;
    // False for the project's level, which is not edited while the game runs;
    // a prefab can be.
    bool mEditableWhileRunning = false;
};

// View of an editor
class UserInterfaceWindowBase
{
public:
    // On the project's open level.
    UserInterfaceWindowBase( BubbleEditor& editor );
    UserInterfaceWindowBase( BubbleEditor& editor, const EditorDocument& document );

protected:
    // Whether the document takes edits now.
    bool Editable() const;

    Window& mWindow;
    EditorMode& mEditorMode;
    Framebuffer& mSceneViewport;
    Framebuffer& mEntityIdViewport;
    SceneCamera& mSceneCamera;

    Project& mProject;
    // The document's: the open level's, or a prefab's.
    Level& mLevel;
    Selection& mSelection;
    History& mHistory;
    Clipboard& mClipboard;
    EntityIdPicker& mPicker;
    bool& mPendingRectSelect;
    bool& mViewportHovered;
    bool& mViewManipulatorUsing;
    bool mEditableWhileRunning = false;

    // For invoking operators from a menu item: now, or - for anything that
    // replaces the level or project being drawn - after this frame.
    OperatorContext Operators() const;
    OperatorQueue& mOperatorQueue;

    // UI global state
    UIGlobals& mUIGlobals;
    // Persisted editor preferences
    EditorSettings& mEditorSettings;
};

}
