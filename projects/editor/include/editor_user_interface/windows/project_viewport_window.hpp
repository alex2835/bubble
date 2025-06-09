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
class ProjectViewportWindow : public UserInterfaceWindowBase
{
public:
    ProjectViewportWindow( BubbleEditor& editorState );
    ~ProjectViewportWindow();

    string_view Name();

    void OnUpdate( DeltaTime );

    uvec2 GlobalToWindowPos( ImVec2 pos );
    uvec2 CaptureWidnowMousePos();

    void ProcessScreenSelectedEntity();
    void ProcessSreenSelectionRect();

    void DrawViewport();
    void DrawGizmoOneEntity( Entity entity );
    void DrawGizmoManyEntities( set<Entity>& entities, TransformComponent& transform );
    //bool DrawViewManipulator();
    void OnDraw( DeltaTime );


private:
    uvec2 mNewSize;
    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::MODE::LOCAL;

    // Screen selection
    bool mIsSelecting = false;
    ImVec2 mStartSelection;
};

}
