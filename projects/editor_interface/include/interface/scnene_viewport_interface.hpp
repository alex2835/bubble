#pragma once
#include "imgui.h"
#include "engine/utils/ieditor_interface.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{
class SceneViewportInterface : public IEditorInterface
{
public:
    SceneViewportInterface( Window& window,
                            Framebuffer& viewport,
                            SceneCamera& camera )
        : mNewSize( 640, 640 ),
          mWindow( window ),
          mSceneViewport( viewport ),
          mSceneCamera( camera )
    {
    
    }

    ~SceneViewportInterface() override
    {

    }

    string_view Name() override
    {
        return "Viewport";
    }

    void OnInit() override
    {

    }

    void OnUpdate( DeltaTime ) override
    {
        if ( mNewSize != mSceneViewport.Size() )
            mSceneViewport.Resize( mNewSize );
    }

    void OnDraw( Engine& ) override
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoTitleBar );
        {
            vec2 viewportSize = mSceneViewport.Size();
            ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

            u64 textureId = mSceneViewport.GetColorAttachmentRendererID();
            ImVec2 textureSize = ImVec2( (float)mSceneViewport.GetWidth(),
                                         (float)mSceneViewport.GetHeight() );
            ImGui::Image( (ImTextureID)textureId, textureSize,
                          ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
            mNewSize = { imguiViewportSize.x, imguiViewportSize.y };

            // Process if hovered
            auto middleButton = mWindow.IsKeyPressed( MouseKey::BUTTON_MIDDLE );
            mSceneCamera.mIsActive = middleButton;
            mWindow.LockCursor( middleButton );
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    uvec2 mNewSize;
    Window& mWindow;
    Framebuffer& mSceneViewport;
    SceneCamera& mSceneCamera;
};

}
