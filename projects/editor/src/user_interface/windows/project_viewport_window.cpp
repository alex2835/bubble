#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/project/project_tree.hpp"
#include <glm/gtc/epsilon.hpp>
#include <imgui.h>
#include <cmath>

namespace bubble
{
ProjectViewportWindow::ProjectViewportWindow( BubbleEditor& editorState )
    : UserInterfaceWindowBase( editorState )
{
    mNewSize = mSceneViewport.Size();
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
    if ( mNewSize != mSceneViewport.Size() )
    {
        mEntityIdViewport.Resize( mNewSize );
        mSceneViewport.Resize( mNewSize );
    }
}

uvec2 ProjectViewportWindow::GlobalToWindowPos( ImVec2 pos )
{
    auto windowPos = ImGui::GetCursorScreenPos();
    auto mouseInWindowPos = uvec2{ std::max( 0, i32(pos.x - windowPos.x) ),
                                   std::max( 0, i32(windowPos.y - pos.y) ) };
    return mouseInWindowPos;
}

uvec2 ProjectViewportWindow::CaptureWidnowMousePos()
{
    auto windowPos = ImGui::GetCursorScreenPos();
    auto mousePos = ImGui::GetMousePos();
    auto mouseInWindowPos = uvec2{ mousePos.x - windowPos.x, windowPos.y - mousePos.y };
    return mouseInWindowPos;
}


void ProjectViewportWindow::ProcessScreenSelectedEntity()
{
    if ( not ImGuizmo::IsUsing() and 
         ImGui::IsWindowHovered() and
         ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) )
    {
        const auto clickPos = CaptureWidnowMousePos();
        const auto pixel = mEntityIdViewport.ReadColorAttachmentPixelRedUint( clickPos );

        mSelection.Clear();
        if ( pixel > 0 )
            mSelection.AddEntity( mProject.mScene.GetEntityById( pixel ), mProject.mScene );
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
        const auto& pixels = mEntityIdViewport.ReadColorAttachmentPixelRedUint( startPos, endPos );
        if ( pixels.empty() )
            return;

        mSelection.Clear();
        set<Entity> entities;
        for ( int i = 0; i < pixels.size(); i+=3 )
        {
            auto pixel = pixels[i];
            // Ignore invalid entities
            if ( pixel > 0 )
            {
                auto entity = mProject.mScene.GetEntityById( pixel );
                entities.insert( entity );
            }
        }
        mSelection.AddEntities( entities, mProject.mScene );
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

    u64 textureId = mSceneViewport.ColorAttachment().RendererID();
    ImVec2 textureSize = ImVec2( (float)mSceneViewport.Width(),
                                 (float)mSceneViewport.Height() );

    ImGui::Image( (ImTextureID)textureId, textureSize, ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
    mNewSize = ivec2( imguiViewportSize.x, imguiViewportSize.y );

    if ( ImGui::IsItemHovered() )
        mSceneCamera.mIsActive = mWindow.IsKeyPressed( MouseKey::RIGHT );
}


void ProjectViewportWindow::DrawGizmoOneEntity( Entity entity )
{
    if ( not mProject.mScene.HasComponent<TransformComponent>( entity ) )
        return;

    auto& entityTransform = mProject.mScene.GetComponent<TransformComponent>( entity );
    auto rotaion = glm::degrees( entityTransform.mRotation );
    mat4 transformNew;
    ImGuizmo::RecomposeMatrixFromComponents( glm::value_ptr( entityTransform.mPosition ),
                                             glm::value_ptr( rotaion ),
                                             glm::value_ptr( entityTransform.mScale ),
                                             glm::value_ptr( transformNew ) );


    const auto lookAt = mSceneCamera.GetLookatMat();
    const auto projection = mSceneCamera.GetProjectionMat( mNewSize.x, mNewSize.y );

    ImGuizmo::Manipulate( glm::value_ptr( lookAt ),
                          glm::value_ptr( projection ),
                          mCurrentGizmoOperation,
                          mCurrentGizmoMode,
                          glm::value_ptr( transformNew ) );


    ImGuizmo::DecomposeMatrixToComponents( glm::value_ptr( transformNew ),
                                           glm::value_ptr( entityTransform.mPosition ),
                                           glm::value_ptr( rotaion ),
                                           glm::value_ptr( entityTransform.mScale ) );

    entityTransform.mRotation = glm::radians( rotaion );
}


void ProjectViewportWindow::DrawGizmoManyEntities( const set<Entity>& entities, Transform& transform )
{
    mat4 transformNew;
    ImGuizmo::RecomposeMatrixFromComponents( glm::value_ptr( transform.mPosition ),
                                             glm::value_ptr( glm::degrees( transform.mRotation ) ),
                                             glm::value_ptr( transform.mScale ),
                                             glm::value_ptr( transformNew ) );

    auto lookAt = mSceneCamera.GetLookatMat();
    auto projection = mSceneCamera.GetProjectionMat( mNewSize.x, mNewSize.y );
    ImGuizmo::Manipulate( glm::value_ptr( lookAt ),
                          glm::value_ptr( projection ),
                          mCurrentGizmoOperation,
                          mCurrentGizmoMode,
                          glm::value_ptr( transformNew ) );
    
    vec3 posNew, rotNew, scaleNew;
    ImGuizmo::DecomposeMatrixToComponents( glm::value_ptr( transformNew ),
                                           glm::value_ptr( posNew ),
                                           glm::value_ptr( rotNew ),
                                           glm::value_ptr( scaleNew ) );

    for ( auto entity : entities )
    {
        if ( not mProject.mScene.HasComponent<TransformComponent>( entity ) )
            continue;
        auto& trans = mProject.mScene.GetComponent<TransformComponent>( entity );
        trans.mPosition += posNew - mSelection.GetGroupTransform().mPosition;
        trans.mRotation += glm::radians( rotNew ) - mSelection.GetGroupTransform().mRotation;
        trans.mScale += scaleNew - mSelection.GetGroupTransform().mScale;
    }

    mSelection.GetGroupTransform().mPosition = posNew;
    mSelection.GetGroupTransform().mRotation = glm::radians( rotNew );
    mSelection.GetGroupTransform().mScale = scaleNew;
}



bool ProjectViewportWindow::DrawViewManipulator()
{
    float distance = 1.0f;
    // Calculate distance based on selected entity's position
    if ( mSelection.IsSingleSelection() )
    {
        auto entity = mSelection.GetSingleEntity();
        if ( mProject.mScene.HasComponent<TransformComponent>( entity ) )
        {
            const auto& entityTransform = mProject.mScene.GetComponent<TransformComponent>( entity );
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
    float viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
    float viewManipulateTop = ImGui::GetWindowPos().y;
    auto manipulatorSize = ImVec2( 128, 128 );
    auto manipulatorPos = ImVec2( viewManipulateRight - manipulatorSize.x, viewManipulateTop );

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
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
    ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
    {
        mUIGlobals.mIsViewportHovered = ImGui::IsWindowHovered();
        DrawViewport();

        if ( mEditorMode == EditorMode::Editing )
        {
            /// Gizmo
            ImGuizmo::SetDrawlist();
            auto windowPos = ImGui::GetWindowPos();
            ImGuizmo::SetRect( windowPos.x, windowPos.y, (f32)mNewSize.x, (f32)mNewSize.y );

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
            mUIGlobals.mIsViewManipulatorUsing = viewManipulatorUsing;

            if ( not viewManipulatorUsing )
            {
                ProcessScreenSelectedEntity();
                ProcessSreenSelectionRect();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

}