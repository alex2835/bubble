#pragma once
#include "editor_user_interface/windows/menubar.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "editor_user_interface/windows/project_tree_window.hpp"
#include "editor_user_interface/windows/project_files_window.hpp"

namespace bubble
{
class BubbleEditor;

class EditorUserInterface
{
public:
    EditorUserInterface( BubbleEditor& editorState );
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    Menubar mMenubar;
    ProjectTreeWindow mEntitiesWindow;
    ProjectViewportWindow mSceneViewportWindow;
    ProjectFilesWindow mProjectWindow;
};

}
