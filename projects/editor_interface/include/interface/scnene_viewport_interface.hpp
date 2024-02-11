#pragma once
#include "imgui.h"
#include "ieditor_interface.hpp"
#include "editor_app.hpp"

namespace bubble
{
class SceneViewportInterface : public IEditorInterface
{
public:
    using IEditorInterface::IEditorInterface;

    ~SceneViewportInterface() override
    {

    }

    string_view Name() override
    {
        return "Viewport";
    }

    void OnInit() override
    {
        mNewSize = mEditorState.sceneViewport.Size();
    }

    void OnUpdate( DeltaTime ) override
    {
        if ( mNewSize != mEditorState.sceneViewport.Size() )
            mEditorState.sceneViewport.Resize( mNewSize );
    }

    ImVec2 CaptureWidnowMousePos()
    {
        auto windowSize = ImGui::GetWindowSize();
        auto windowPos = ImGui::GetCursorScreenPos();
        auto mousePos = ImGui::GetMousePos();
        auto mouseInWindowPos = ImVec2{ mousePos.x - windowPos.x, windowPos.y - mousePos.y };
        auto mouseInWindowPosRel = mouseInWindowPos / windowSize;
        std::cout << mouseInWindowPosRel.x << " " << mouseInWindowPosRel.y << std::endl;
        return mouseInWindowPosRel;
    }


    void DrawViewport()
    {
        vec2 viewportSize = mEditorState.sceneViewport.Size();
        ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

        u64 textureId = mEditorState.sceneViewport.GetColorAttachmentRendererID();
        ImVec2 textureSize = ImVec2( (float)mEditorState.sceneViewport.GetWidth(),
                                     (float)mEditorState.sceneViewport.GetHeight() );

        ImGui::Image( (ImTextureID)textureId, textureSize, ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
        mNewSize = ivec2( imguiViewportSize.x, imguiViewportSize.y );

        if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) )
            auto pos = CaptureWidnowMousePos();

        if ( ImGui::IsItemHovered() )
        {
            auto middleButton = mEditorState.window.IsKeyPressed( MouseKey::BUTTON_MIDDLE );
            mEditorState.sceneCamera.mIsActive = middleButton;
            mEditorState.window.LockCursor( middleButton );
        }
    }

    void DrawGizmo()
    {
        if ( ImGui::IsKeyPressed( ImGuiKey_T ) )
            mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if ( ImGui::IsKeyPressed( ImGuiKey_E ) )
            mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if ( ImGui::IsKeyPressed( ImGuiKey_R ) )
            mCurrentGizmoOperation = ImGuizmo::SCALE;


        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect( ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                           (float)mNewSize.x, (float)mNewSize.y );

        auto lookAt = mEditorState.sceneCamera.GetLookatMat();
        auto projection = mEditorState.sceneCamera.GetPprojectionMat( mNewSize.x, mNewSize.y );


        mat4 matrix;
        auto& entityTransform = mEditorState.selectedEntity.GetComponent<TransformComponent>();

        ImGuizmo::RecomposeMatrixFromComponents( glm::value_ptr( entityTransform.mPosition ),
                                                 glm::value_ptr( entityTransform.mRotation ),
                                                 glm::value_ptr( entityTransform.mScale ),
                                                 glm::value_ptr( matrix ) );

        ImGuizmo::Manipulate( glm::value_ptr( lookAt ), 
                              glm::value_ptr( projection ),
                              mCurrentGizmoOperation, 
                              mCurrentGizmoMode,
                              glm::value_ptr( matrix ) );

        ImGuizmo::DecomposeMatrixToComponents( glm::value_ptr( matrix ),
                                               glm::value_ptr( entityTransform.mPosition ),
                                               glm::value_ptr( entityTransform.mRotation ),
                                               glm::value_ptr( entityTransform.mScale ) );
    }

    void OnDraw( DeltaTime ) override
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse );
        {
            DrawViewport();
            DrawGizmo();
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    uvec2 mNewSize;

    ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::MODE::LOCAL;
};

}
