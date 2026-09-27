#pragma once
#include "editor_user_interface/windows/menubar.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_user_interface/windows/project_files_window.hpp"
#include "editor_user_interface/windows/console_window.hpp"
#include "editor_user_interface/windows/animation_graph_window.hpp"
#include "editor_user_interface/windows/prefab_editor_window.hpp"

namespace bubble
{
class BubbleEditor;

class EditorUserInterface
{
public:
    EditorUserInterface( BubbleEditor& editorState );
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );
    // The scene views that are not the level's, drawn before the UI.
    void Render( Engine& engine, DeltaTime dt );

    PrefabEditorWindow& PrefabEditor() { return mPrefabEditorWindow; }
    std::optional<OperatorContext> FocusedPrefabDocument() { return mPrefabEditorWindow.FocusedDocument(); }

private:
    Menubar mMenubar;
    ProjectTreeWindow mEntitiesWindow;
    ProjectViewportWindow mSceneViewportWindow;
    ProjectFilesWindow mProjectWindow;
    ConsoleWindow mConsoleWindow;
    AnimationGraphWindow mAnimationGraphWindow;
    PrefabEditorWindow mPrefabEditorWindow;
};

}
