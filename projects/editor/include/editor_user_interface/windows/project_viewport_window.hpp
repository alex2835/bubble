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
    // Consumes a finished entity id readback, if one has arrived.
    void ResolvePendingSelection();
    void ProcessSreenSelectionRect();

    void DrawViewport();
    void DrawGizmoOneEntity( Entity entity );
    void DrawGizmoManyEntities( const set<Entity>& entities, Transform& transform );
    bool DrawViewManipulator();
    void OnDraw( DeltaTime );


private:
    // Size represent actual viewport size
    uvec2 mSize;
    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::MODE::LOCAL;

    // Top left of the viewport image in screen coordinates, captured when it is
    // drawn. Picking maps mouse positions through this.
    ImVec2 mViewportScreenMin = ImVec2( 0, 0 );

    // Screen selection
    bool mIsSelecting = false;
    ImVec2 mStartSelection;

    // View manipulator state
    mat4 mLastLookAtMatrix = mat4( 1.0f );
    bool mViewManipulatorWasUsing = false;

    // Gizmo transform tracking for undo/redo
    bool mGizmoWasUsing = false;
    Transform mGizmoStartTransform;
    map<Entity, Transform> mGizmoStartTransforms;
};

}
