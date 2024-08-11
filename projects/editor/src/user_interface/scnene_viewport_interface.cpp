
#include "editor_interfaces/interfaces/scnene_viewport_interface.hpp"

namespace bubble
{
SceneViewportInterface::SceneViewportInterface( EditorState& editorState )
    : IEditorInterface( editorState )
{
    mNewSize = mSceneViewport.Size();
}

SceneViewportInterface::~SceneViewportInterface()
{

}

bubble::string_view SceneViewportInterface::Name()
{
    return "Viewport"sv;
}

void SceneViewportInterface::OnUpdate( DeltaTime )
{
    if ( mNewSize != mSceneViewport.Size() )
    {
        mObjectIdViewport.Resize( mNewSize );
        mSceneViewport.Resize( mNewSize );
    }
}

glm::uvec2 SceneViewportInterface::CaptureWidnowMousePos()
{
    auto windowSize = ImGui::GetWindowSize();
    auto windowPos = ImGui::GetCursorScreenPos();
    auto mousePos = ImGui::GetMousePos();
    auto mouseInWindowPos = uvec2{ mousePos.x - windowPos.x, windowPos.y - mousePos.y };
    return mouseInWindowPos;
}

void SceneViewportInterface::ProcessScreenSelectedEntity()
{
    if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) )
    {
        auto clickPos = CaptureWidnowMousePos();
        auto pixel =
            mObjectIdViewport.
            ReadColorAttachmentPixelRedUint( clickPos );
        mSelectedEntity = mProject.mScene.GetEntityById( pixel );
    }
}

void SceneViewportInterface::DrawViewport()
{
    vec2 viewportSize = mSceneViewport.Size();
    ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

    u64 textureId = mSceneViewport.ColorAttachment().RendererID();
    ImVec2 textureSize = ImVec2( (float)mSceneViewport.Width(),
                                 (float)mSceneViewport.Height() );

    ImGui::Image( (ImTextureID)textureId, textureSize, ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
    mNewSize = ivec2( imguiViewportSize.x, imguiViewportSize.y );

    if ( ImGui::IsItemHovered() )
    {
        auto middleButton = mWindow.IsKeyPressed( MouseKey::BUTTON_MIDDLE );
        mSceneCamera.mIsActive = middleButton;
    }
}

void SceneViewportInterface::DrawGizmo()
{
    if ( ImGui::IsWindowFocused() )
    {
        if ( ImGui::IsKeyPressed( ImGuiKey_T ) )
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if ( ImGui::IsKeyPressed( ImGuiKey_E ) )
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if ( ImGui::IsKeyPressed( ImGuiKey_R ) )
            mCurrentGizmoOperation = ImGuizmo::SCALE;
    }
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect( ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                       (float)mNewSize.x, (float)mNewSize.y );

    mat4 transformMat;
    auto& entityTransform = mSelectedEntity.GetComponent<TransformComponent>();
    auto lookAt = mSceneCamera.GetLookatMat();
    auto projection = mSceneCamera.GetPprojectionMat( mNewSize.x, mNewSize.y );

    ImGuizmo::RecomposeMatrixFromComponents( glm::value_ptr( entityTransform.mPosition ),
                                             glm::value_ptr( entityTransform.mRotation ),
                                             glm::value_ptr( entityTransform.mScale ),
                                             glm::value_ptr( transformMat ) );

    ImGuizmo::Manipulate( glm::value_ptr( lookAt ),
                          glm::value_ptr( projection ),
                          mCurrentGizmoOperation,
                          mCurrentGizmoMode,
                          glm::value_ptr( transformMat ) );

    ImGuizmo::DecomposeMatrixToComponents( glm::value_ptr( transformMat ),
                                           glm::value_ptr( entityTransform.mPosition ),
                                           glm::value_ptr( entityTransform.mRotation ),
                                           glm::value_ptr( entityTransform.mScale ) );
}

void SceneViewportInterface::OnDraw( DeltaTime )
{
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
    ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
    {
        DrawViewport();
        if ( mSelectedEntity &&
             mSelectedEntity.HasComponent<TransformComponent>() )
            DrawGizmo();

        // Select entity on click by pixel
        if ( !ImGuizmo::IsUsing() && ImGui::IsWindowHovered() )
            ProcessScreenSelectedEntity();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

}