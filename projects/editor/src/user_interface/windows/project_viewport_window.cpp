#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/project/project_tree.hpp"
#include "engine/editing/commands/transform_commands.hpp"
#include <glm/gtc/epsilon.hpp>
#include <imgui.h>
#include <cmath>
#include "engine/scene/components/transform_component.hpp"
#include "engine/scene/hierarchy.hpp"
#include <nlohmann/json.hpp>
#include "engine/serialization/types_serialization.hpp"
#include "engine/editing/operators/operator.hpp"

namespace bubble
{
ProjectViewportWindow::ProjectViewportWindow( BubbleEditor& editorState )
    : ProjectViewportWindow( editorState, editorState.MainDocument() )
{
}

ProjectViewportWindow::ProjectViewportWindow( BubbleEditor& editorState, const EditorDocument& document )
    : UserInterfaceWindowBase( editorState, document )
{
    static int sGizmoIds = 0;
    mGizmoId = ++sGizmoIds;
    mSize = mSceneViewport.Size();
}


ProjectViewportWindow::~ProjectViewportWindow()
{

}


string_view ProjectViewportWindow::Name()
{
    return "Viewport"sv;
}


void ProjectViewportWindow::OnUpdate( DeltaTime )
{
    if ( mSize != mSceneViewport.Size() )
    {
        // Resizing recreates the id attachment, so anything reading from it is
        // now pointing at a texture that no longer exists.
        mPicker.Cancel();
        mEntityIdViewport.Resize( mSize );
        mSceneViewport.Resize( mSize );
    }

    // A readback lands one or two frames after the click. Resolving it here,
    // after the UI has drawn, matches when the selection used to take effect:
    // the gizmo and the hotkeys both read mSelection before the click that set
    // it was ever processed, so this was always a frame behind.
    ResolvePendingSelection();
}

void ProjectViewportWindow::ResolvePendingSelection()
{
    vector<u32> pixels;
    uvec2 size( 0u );
    if ( not mPicker.TakeResult( pixels, size ) )
        return;
    if ( pixels.empty() )
        return;

    mSelection.Clear();

    if ( mPendingRectSelect )
    {
        set<Entity> entities;
        // Every pixel, not every third. The attachment is single channel
        // R32Uint, so one u32 is one pixel - the old stride of 3 was left over
        // from thinking in RGB and quietly dropped two thirds of a selection.
        for ( u64 i = 0; i < pixels.size(); i++ )
        {
            if ( pixels[i] > 0 )
                entities.insert( mLevel.mScene.GetEntityById( pixels[i] ) );
        }
        mSelection.AddEntities( entities, mLevel.mScene );
    }
    else if ( pixels[0] > 0 )
    {
        mSelection.AddEntity( mLevel.mScene.GetEntityById( pixels[0] ), mLevel.mScene );
    }
}

// Screen position to a pixel in the viewport's attachments.
//
// Measured down from the top left corner, because that is where a WebGPU render
// target starts. Under OpenGL this subtracted the other way round to flip into a
// bottom left origin.
uvec2 ProjectViewportWindow::GlobalToWindowPos( ImVec2 pos )
{
    return uvec2{ (u32)std::max( 0, i32( pos.x - mViewportScreenMin.x ) ),
                  (u32)std::max( 0, i32( pos.y - mViewportScreenMin.y ) ) };
}

uvec2 ProjectViewportWindow::CaptureWidnowMousePos()
{
    return GlobalToWindowPos( ImGui::GetMousePos() );
}


void ProjectViewportWindow::ProcessScreenSelectedEntity()
{
    if ( not ImGuizmo::IsUsing() and 
         ImGui::IsWindowHovered() and
         ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) )
    {
        // Only asks for the read. The id pass is rendered next frame and the
        // result arrives after that, through ResolvePendingSelection.
        const auto clickPos = CaptureWidnowMousePos();
        mPendingRectSelect = false;
        mPicker.Request( clickPos, clickPos );
    }
}

void ProjectViewportWindow::ProcessSreenSelectionRect()
{
    // Start
    if ( not ImGuizmo::IsUsing() and
         ImGui::IsWindowHovered() and
         ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) )
    {
        mIsSelecting = true;
        mStartSelection = ImGui::GetMousePos();
    }

    // End
    if ( mIsSelecting and
         ( not ImGui::IsWindowHovered() or
           ImGui::IsMouseReleased( ImGuiMouseButton_Left ) ) )
    {
        mIsSelecting = false;
        auto startPos = GlobalToWindowPos( mStartSelection );
        auto endPos = GlobalToWindowPos( ImGui::GetMousePos() );
        mPendingRectSelect = true;
        mPicker.Request( startPos, endPos );
    }

    // selection rect
    if ( mIsSelecting )
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p1 = mStartSelection;
        ImVec2 p2 = ImGui::GetMousePos();
        ImU32 color = IM_COL32( 255, 100, 20, 100 );
        drawList->AddRectFilled( p1, p2, color, 0.0f );
    }
}


void ProjectViewportWindow::DrawViewport()
{
    vec2 viewportSize = mSceneViewport.Size();
    ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

    u64 textureId = mSceneViewport.ColorAttachment().ImTextureId();
    ImVec2 textureSize = ImVec2( (float)mSceneViewport.Width(),
                                 (float)mSceneViewport.Height() );

    // Drawn with default UVs. The flipped ones this used to pass were correcting
    // for OpenGL, where a rendered texture's first row is its bottom. WebGPU's
    // framebuffer origin is the top left, so the image already arrives the right
    // way up and flipping it turns the camera upside down.
    ImGui::Image( (ImTextureID)textureId, textureSize );

    // The image's screen rectangle, for turning a mouse position into a pixel in
    // the id buffer. Taken from the item rather than the ImGui cursor: the cursor
    // has advanced past the image by the time picking runs, so reading it there
    // gave the bottom edge and only worked because the Y math was inverted too.
    mViewportScreenMin = ImGui::GetItemRectMin();
    mSize = ivec2( imguiViewportSize.x, imguiViewportSize.y );

    // A prefab dropped on the view lands in front of the camera - at the
    // origin, in the prefab editor.
    if ( Editable() and ImGui::BeginDragDropTarget() )
    {
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "PREFAB_FILE" ) )
        {
            const vec3 spawnAt = mEditableWhileRunning ? vec3( 0 ) : mSceneCamera.mPosition + mSceneCamera.mForward * 30.0f;
            try
            {
                OperatorContext ctx = Operators();
                InvokeOperator( "prefab.instantiate", ctx, { { "file", string( (const char*)payload->Data ) }, { "spawn_at", spawnAt } } );
            }
            catch ( const std::exception& e )
            {
                LogError( "prefab.instantiate: {}", e.what() );
            }
        }
        ImGui::EndDragDropTarget();
    }

    if ( ImGui::IsItemHovered() )
        mSceneCamera.mIsActive = mWindow.IsKeyPressed( MouseKey::RIGHT );
}


ImGuizmo::MODE ProjectViewportWindow::GizmoMode()
{
    if ( not ImGuizmo::IsUsing() )
        mDragGizmoMode = ImGui::GetIO().KeyShift ? ImGuizmo::WORLD : mCurrentGizmoMode;
    return mDragGizmoMode;
}

void ProjectViewportWindow::DrawGizmoOneEntity( Entity entity )
{
    if ( not mLevel.mScene.HasComponent<TransformComponent>( entity ) )
        return;

    auto& entityTransform = mLevel.mScene.GetComponent<TransformComponent>( entity );

    // Check if gizmo just started being used
    bool isUsing = ImGuizmo::IsUsing();
    if ( isUsing and not mGizmoWasUsing )
    {
        // Store initial transform for undo/redo
        mGizmoStartTransform = entityTransform;
    }

    // The gizmo moves the world matrix; the local transform is worked back
    // out of it against the parent. Only what the gizmo changed is written,
    // so an untouched part keeps its exact value rather than a round trip
    // through the matrix.
    Scene& scene = mLevel.mScene;
    mat4 transformNew = ComputeWorldMatrix( scene, entity );


    const auto lookAt = mSceneCamera.GetLookatMat();
    const auto projection = mSceneCamera.GetProjectionMat( mSize.x, mSize.y );

    ImGuizmo::Manipulate( glm::value_ptr( lookAt ),
                          glm::value_ptr( projection ),
                          mCurrentGizmoOperation,
                          GizmoMode(),
                          glm::value_ptr( transformNew ) );


    if ( ImGuizmo::IsUsing() )
    {
        const Entity parent = ParentOf( scene, entity );
        const Transform moved = Transform::FromMatrix( parent == INVALID_ENTITY
                                                       ? transformNew
                                                       : glm::inverse( ComputeWorldMatrix( scene, parent ) ) * transformNew );
        if ( mCurrentGizmoOperation & ImGuizmo::TRANSLATE )
            entityTransform.mPosition = moved.mPosition;
        if ( mCurrentGizmoOperation & ImGuizmo::ROTATE )
            entityTransform.mRotation = moved.mRotation;
        if ( mCurrentGizmoOperation & ImGuizmo::SCALE )
            entityTransform.mScale = moved.mScale;
    }

    // Check if gizmo just stopped being used
    if ( not isUsing and mGizmoWasUsing )
    {
        // The gizmo already moved it; the step is recorded, not re-applied.
        mHistory.Record( CreateScope<TransformChangeCommand>( entity, mLevel.mScene,
                                                              mGizmoStartTransform, Transform( entityTransform ) ) );
    }

    mGizmoWasUsing = isUsing;
}


void ProjectViewportWindow::DrawGizmoManyEntities( const set<Entity>& entities, Transform& transform )
{
    // Check if gizmo just started being used
    bool isUsing = ImGuizmo::IsUsing();
    if ( isUsing and not mGizmoWasUsing )
    {
        // Store initial transforms for all entities
        mGizmoStartTransforms.clear();
        for ( auto entity : entities )
        {
            if ( mLevel.mScene.HasComponent<TransformComponent>( entity ) )
            {
                mGizmoStartTransforms[entity] = mLevel.mScene.GetComponent<TransformComponent>( entity );
            }
        }
    }

    mat4 transformNew = transform.TransformMat();

    auto lookAt = mSceneCamera.GetLookatMat();
    auto projection = mSceneCamera.GetProjectionMat( mSize.x, mSize.y );
    ImGuizmo::Manipulate( glm::value_ptr( lookAt ),
                          glm::value_ptr( projection ),
                          mCurrentGizmoOperation,
                          GizmoMode(),
                          glm::value_ptr( transformNew ) );

    // What the gizmo did to the group this frame, laid onto each entity in
    // the world: the move added, the turn applied on top of its own
    // rotation; the scale is added to its local one. An entity whose parent
    // is selected too is left to follow that parent, or it would move twice.
    Scene& scene = mLevel.mScene;
    const Transform moved = ImGuizmo::IsUsing() ? Transform::FromMatrix( transformNew ) : transform;
    const vec3 positionDelta = moved.mPosition - transform.mPosition;
    const quat rotationDelta = moved.mRotation * glm::inverse( transform.mRotation );
    const vec3 scaleDelta = moved.mScale - transform.mScale;

    for ( auto entity : entities )
    {
        if ( not scene.HasComponent<TransformComponent>( entity ) )
            continue;
        const bool ancestorSelected = std::ranges::any_of( entities, [&]( Entity other )
        {
            return other != entity and IsAncestor( scene, other, entity );
        } );
        if ( ancestorSelected )
            continue;
        Transform world = Transform::FromMatrix( ComputeWorldMatrix( scene, entity ) );
        world.mPosition += positionDelta;
        world.mRotation = glm::normalize( rotationDelta * world.mRotation );
        const vec3 localScale = scene.GetComponent<TransformComponent>( entity ).mScale + scaleDelta;
        SetWorldTransform( scene, entity, world );
        scene.GetComponent<TransformComponent>( entity ).mScale = localScale;
    }

    transform = moved;

    // Check if gizmo just stopped being used
    if ( not isUsing and mGizmoWasUsing )
    {
        // Collect end transforms
        map<Entity, Transform> endTransforms;
        for ( auto entity : entities )
        {
            if ( mLevel.mScene.HasComponent<TransformComponent>( entity ) )
            {
                endTransforms[entity] = mLevel.mScene.GetComponent<TransformComponent>( entity );
            }
        }

        mHistory.Record( CreateScope<MultiTransformChangeCommand>( entities, mLevel.mScene,
                                                                   mGizmoStartTransforms, endTransforms ) );
    }

    mGizmoWasUsing = isUsing;
}



bool ProjectViewportWindow::DrawViewManipulator()
{
    float distance = 1.0f;
    // Calculate distance based on selected entity's position
    if ( mSelection.IsSingleSelection() )
    {
        auto entity = mSelection.GetSingleEntity();
        if ( mLevel.mScene.HasComponent<TransformComponent>( entity ) )
        {
            const auto& entityTransform = mLevel.mScene.GetComponent<TransformComponent>( entity );
            distance = std::round( glm::distance( entityTransform.mPosition, mSceneCamera.mPosition ) );
        }
    }
    else if ( mSelection.IsMultiSelection() )
    {
        // Use group transform position for multi-selection
        distance = std::round( glm::distance( mSelection.GetGroupTransform().mPosition, mSceneCamera.mPosition ) );
    }

    // Get current lookAt matrix from camera
    mat4 lookAt = mSceneCamera.GetLookatMat();

    // If not actively manipulating, store current matrix as reference
    if ( not mViewManipulatorWasUsing )
    {
        mLastLookAtMatrix = lookAt;
    }

    float windowWidth = ImGui::GetWindowWidth();
    float windowHeight = ImGui::GetWindowHeight();
    float viewManipulateLeft = ImGui::GetWindowPos().x;
    float viewManipulateTop = ImGui::GetWindowPos().y;
    auto manipulatorSize = ImVec2( 128, 128 );
    auto manipulatorPos = ImVec2( viewManipulateLeft, viewManipulateTop );

    // ViewManipulate modifies the lookAt matrix in place
    bool isUsing = ImGuizmo::ViewManipulate( glm::value_ptr( lookAt ), distance, manipulatorPos, manipulatorSize, 0x10101010 );

    // Only apply changes when actively being used
    if ( isUsing )
    {
        // Check if matrix actually changed to avoid unnecessary updates
        constexpr float epsilon = 0.0001f;
        bool matrixChanged = false;
        for ( int i = 0; i < 4 && !matrixChanged; ++i )
        {
            for ( int j = 0; j < 4; ++j )
            {
                if ( std::abs( lookAt[i][j] - mLastLookAtMatrix[i][j] ) > epsilon )
                {
                    matrixChanged = true;
                    break;
                }
            }
        }

        if ( matrixChanged )
        {
            // Extract the updated camera orientation from the modified lookAt matrix
            auto inverse = glm::inverse( lookAt );

            // Extract directional vectors from inverse view matrix
            mSceneCamera.mForward = -normalize( vec3( inverse[2] ) );
            mSceneCamera.mUp = normalize( vec3( inverse[1] ) );
            mSceneCamera.mRight = normalize( vec3( inverse[0] ) );
            mSceneCamera.mPosition = vec3( inverse[3] );

            // Update Euler angles to match new orientation
            // Reverse of: forward.x = cos(yaw) * cos(pitch), forward.z = sin(yaw) * cos(pitch)
            mSceneCamera.mYaw = std::atan2( mSceneCamera.mForward.z, mSceneCamera.mForward.x );
            // Reverse of: forward.y = sin(pitch)
            // Clamp to avoid NaN from asin
            float pitchSin = glm::clamp( mSceneCamera.mForward.y, -1.0f, 1.0f );
            mSceneCamera.mPitch = std::asin( pitchSin );

            // Store the new matrix
            mLastLookAtMatrix = lookAt;
        }
    }

    // Track whether manipulator was being used for next frame
    mViewManipulatorWasUsing = isUsing;

    return isUsing;
}


void ProjectViewportWindow::OnDraw( DeltaTime )
{
    if ( not mUIGlobals.mShow.mViewport )
    {
        // Nothing under the mouse to fly the camera or pick with.
        mViewportHovered = false;
        return;
    }
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
    ImGui::Begin( Name().data(), &mUIGlobals.mShow.mViewport, ImGuiWindowFlags_NoCollapse );
    DrawContent();
    ImGui::End();
    ImGui::PopStyleVar();
}

void ProjectViewportWindow::DrawContent()
{
    {
        mViewportHovered = ImGui::IsWindowHovered();
        DrawViewport();

        if ( Editable() )
        {
            // Every viewport's gizmo under its own id: the main one and the
            // prefab editor's are both live in one frame.
            ImGuizmo::SetID( mGizmoId );
            /// Gizmo
            ImGuizmo::SetDrawlist();
            auto windowPos = ImGui::GetWindowPos();
            ImGuizmo::SetRect( windowPos.x, windowPos.y, (f32)mSize.x, (f32)mSize.y );

            if ( not mSelection.GetEntities().empty() )
            {
                if ( ImGui::IsWindowHovered() )
                {
                    if ( ImGui::IsKeyPressed( ImGuiKey_T ) )
                        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                    if ( ImGui::IsKeyPressed( ImGuiKey_E ) )
                        mCurrentGizmoOperation = ImGuizmo::ROTATE;
                    if ( ImGui::IsKeyPressed( ImGuiKey_R ) )
                        mCurrentGizmoOperation = ImGuizmo::SCALE;
                }
            }

            if ( mSelection.GetEntities().size() == 1 )
                DrawGizmoOneEntity( *mSelection.GetEntities().begin() );
            else if ( mSelection.GetEntities().size() > 1 )
                DrawGizmoManyEntities( mSelection.GetEntities(), mSelection.GetGroupTransform() );

            bool viewManipulatorUsing = DrawViewManipulator();
            mViewManipulatorUsing = viewManipulatorUsing;

            if ( not viewManipulatorUsing )
            {
                ProcessScreenSelectedEntity();
                ProcessSreenSelectionRect();
            }
        }
    }
}

}