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
        mNewSize = mEditorState.mSceneViewport.Size();
    }

    void OnUpdate( DeltaTime ) override
    {
        if ( mNewSize != mEditorState.mSceneViewport.Size() )
        {
            mEditorState.mObjectIdViewport.Resize( mNewSize );
            mEditorState.mSceneViewport.Resize( mNewSize );
        }
    }

    void ReadScreenSelectedEntity( uvec2 pos )
    {
        mEditorState.mObjectIdViewport.Bind();
        glcall( glReadBuffer( GL_COLOR_ATTACHMENT0 ) );
        u32 id = 0;
        glcall( glReadPixels( pos.x, pos.y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id ) );
        glcall( glReadBuffer( GL_NONE ) );
        glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
        mEditorState.mSelectedEntity = mEngine.mScene.EntityById( id );
    }

    uvec2 CaptureWidnowMousePos()
    {
        auto windowSize = ImGui::GetWindowSize();
        auto windowPos = ImGui::GetCursorScreenPos();
        auto mousePos = ImGui::GetMousePos();
        auto mouseInWindowPos = uvec2{ mousePos.x - windowPos.x, windowPos.y - mousePos.y };
        return mouseInWindowPos;
    }


    void DrawViewport()
    {
        vec2 viewportSize = mEditorState.mSceneViewport.Size();
        ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

        u64 textureId = mEditorState.mSceneViewport.ColorAttachment().RendererID();
        ImVec2 textureSize = ImVec2( (float)mEditorState.mSceneViewport.Width(),
                                     (float)mEditorState.mSceneViewport.Height() );

        ImGui::Image( (ImTextureID)textureId, textureSize, ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
        mNewSize = ivec2( imguiViewportSize.x, imguiViewportSize.y );


        if ( ImGui::IsItemHovered() )
        {
            auto middleButton = mEditorState.mWindow.IsKeyPressed( MouseKey::BUTTON_MIDDLE );
            mEditorState.mSceneCamera.mIsActive = middleButton;
            mEditorState.mWindow.LockCursor( middleButton );
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

        mat4 matrix;
        auto& entityTransform = mEditorState.mSelectedEntity.GetComponent<TransformComponent>();
        auto lookAt = mEditorState.mSceneCamera.GetLookatMat();
        auto projection = mEditorState.mSceneCamera.GetPprojectionMat( mNewSize.x, mNewSize.y );

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
            if ( mEditorState.mSelectedEntity != INVALID_ENTITY )
                DrawGizmo();

            // Select entity on click
            if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left, false ) &&
                 !ImGuizmo::IsUsing() )
            {
                auto clickPos = CaptureWidnowMousePos();
                ReadScreenSelectedEntity( clickPos );
            }
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
