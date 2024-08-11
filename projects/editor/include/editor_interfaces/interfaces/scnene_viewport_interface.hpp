#pragma once
#include "editor_interfaces/ieditor_interface.hpp"

namespace bubble
{
class SceneViewportInterface : public IEditorInterface
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
