#include "engine/pch/pch.hpp"
#include "editor_user_interface/windows/project_viewport_window.hpp"
#include "editor_application/editor_application.hpp"
#include "engine/project/project_tree.hpp"
#include <glm/gtc/epsilon.hpp>
#include <imgui.h>

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

        mSelection = {};
        if ( pixel > 0 )
            mSelection.mEntities.insert( mProject.mScene.GetEntityById( pixel ) );
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

        float count = 0;
        auto avgPos = vec3( 0 );
        mSelection = {};
        for ( int i = 0; i < pixels.size(); i+=3 )
        {
            auto pixel = pixels[i];
            // Ignore invalid entities
            if ( pixel > 0 )
            {
                auto entity = mProject.mScene.GetEntityById( pixel );
                if ( mSelection.mEntities.insert( entity ).second and
                     mProject.mScene.HasComponent<TransformComponent>( entity ) )
                {
                    count++;
                    avgPos += mProject.mScene.GetComponent<TransformComponent>( entity ).mPosition;
                }
            }
        }
        mSelection.mGroupTransform.mPosition = avgPos * ( 1.0f / count );
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


void ProjectViewportWindow::DrawGizmoManyEntities( set<Entity>& entities, 
                                                   TransformComponent& transform )
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
        trans.mPosition += posNew - mSelection.mGroupTransform.mPosition;
        trans.mRotation += glm::radians( rotNew ) - mSelection.mGroupTransform.mRotation;
        trans.mScale += scaleNew - mSelection.mGroupTransform.mScale;
    }

    mSelection.mGroupTransform.mPosition = posNew;
    mSelection.mGroupTransform.mRotation = glm::radians( rotNew );
    mSelection.mGroupTransform.mScale = scaleNew;
}



//bool SceneViewportInterface::DrawViewManipulator()
//{
//    auto lookAt = mSceneCamera.GetLookatMat();
//
//    float distance = 1.0f;
//    if ( mSelectedEntity and mSelectedEntity.HasComponent<TransformComponent>() )
//    {
//        auto& entityTransform = mSelectedEntity.GetComponent<TransformComponent>();
//        distance = std::round( glm::distance(entityTransform.mPosition, mSceneCamera.mPosition) );
//    }
//
//    float windowWidth = ImGui::GetWindowWidth();
//    float windowHeight = ImGui::GetWindowHeight();
//    float viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
//    float viewManipulateTop = ImGui::GetWindowPos().y;
//    auto manipulatorSize = ImVec2( 128, 128 );
//    auto manipulatorPos = ImVec2( viewManipulateRight - manipulatorSize.x, viewManipulateTop );
//
//    bool isUsing = ImGuizmo::ViewManipulate( glm::value_ptr( lookAt ), distance, manipulatorPos, manipulatorSize, 0x10101010 );
//    if ( isUsing )
//    {
//        // extract changed values
//        auto inverse = glm::inverse( lookAt );
//        mSceneCamera.mFront = -vec3( inverse[2][0], inverse[2][1], inverse[2][2] );
//        mSceneCamera.mUp = vec3( inverse[1][0], inverse[1][1], inverse[1][2] );
//        mSceneCamera.mRight = vec3( inverse[0][0], inverse[0][1], inverse[0][2] );
//        mSceneCamera.mPosition = vec3( inverse[3][0], inverse[3][1], inverse[3][2] );
//        mSceneCamera.mYaw = std::atan2( mSceneCamera.mFront.x, mSceneCamera.mFront.z );
//        mSceneCamera.mPitch = std::atan2( mSceneCamera.mFront.y, 
//                                     std::sqrtf( std::powf(mSceneCamera.mFront.x,2) + 
//                                           std::powf(mSceneCamera.mFront.z,2) ) );
//    }
//    return isUsing;
//}


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

            if ( not mSelection.mEntities.empty() )
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

            if ( mSelection.mEntities.size() == 1 )
                DrawGizmoOneEntity( *mSelection.mEntities.begin() );
            else if ( mSelection.mEntities.size() > 1 )
                DrawGizmoManyEntities( mSelection.mEntities, mSelection.mGroupTransform );

            //bool viewManipulatorUsing = DrawViewManipulator();

            ProcessScreenSelectedEntity();
            ProcessSreenSelectionRect();

        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

}