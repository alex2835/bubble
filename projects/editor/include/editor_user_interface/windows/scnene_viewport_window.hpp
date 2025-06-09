#pragma once
#include <imgui.h>
#include <ImGuizmo.h>
//#include <ImSequencer.h>
//#include <ImZoomSlider.h>
//#include <ImCurveEdit.h>
//#include <GraphEditor.h>
#include "editor_user_interface/windows/window_base.hpp"

namespace bubble
{
class SceneViewportInterface : public UserInterfaceWindowBase
{
public:
    SceneViewportInterface( BubbleEditor& editorState );
    ~SceneViewportInterface();

    string_view Name();

    void OnUpdate( DeltaTime );

    void SetSelection( Entity selectedEtnity );

    uvec2 CaptureWidnowMousePos();
    void ProcessScreenSelectedEntity();

    void DrawViewport();
    void DrawGizmoOneEntity( Entity entity );
    void DrawGizmoManyEntities( vector<Entity>& entities, TransformComponent& transform );
    //bool DrawViewManipulator();
    void OnDraw( DeltaTime );

private:
    uvec2 mNewSize;
    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::MODE::LOCAL;
};

}
