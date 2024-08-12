#pragma once
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class SceneViewportInterface : public UserInterfaceWindowBase
{
public:
    SceneViewportInterface( EditorState& editorState );
    ~SceneViewportInterface();

    string_view Name();

    void OnUpdate( DeltaTime );

    uvec2 CaptureWidnowMousePos();
    void ProcessScreenSelectedEntity();

    void DrawViewport();
    void DrawGizmo();
    void OnDraw( DeltaTime );

private:
    uvec2 mNewSize;
    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::MODE::LOCAL;
};

}
