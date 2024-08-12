#pragma once
#include <imgui.h>
#include "engine/utils/types.hpp"
#include "editor_application/editor_state.hpp"

#include "editor_user_interface/windows/menubar.hpp"
#include "editor_user_interface/windows/entities_window.hpp"
#include "editor_user_interface/windows/scnene_viewport_window.hpp"
#include "editor_user_interface/windows/project_window.hpp"

namespace bubble
{
class EditorUserInterface
{
public:
    EditorUserInterface( EditorState& editorState );
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    Menubar mMenubar;
    EntitiesWindow mEntitiesWindow;
    SceneViewportInterface mSceneViewportWindow;
    ProjectWindow mProjectWindow;
};

}
