#pragma once
#include <imgui.h>
#include "engine/utils/types.hpp"

#include "editor_interfaces/interfaces/menubar.hpp"
#include "editor_interfaces/interfaces/entities_interface.hpp"
#include "editor_interfaces/interfaces/scnene_viewport_interface.hpp"
#include "editor_interfaces/interfaces/project_interface.hpp"

namespace bubble
{
struct EditorState;

class EditorInterfaceHotReloader
{
public:
    EditorInterfaceHotReloader( EditorState& editorState );
    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    Menubar mMenubar;
    EntitiesInterface mEntitiesInterface;
    SceneViewportInterface mSceneViewportInterface;
    ProjectInterface mProjectInterface;
};
}
