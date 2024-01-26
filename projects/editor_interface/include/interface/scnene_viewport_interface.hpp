#pragma once
#include "imgui.h"
#include "engine/utils/ieditor_interface.hpp"
#include "engine/renderer/framebuffer.hpp"

namespace bubble
{
class SceneViewportInterface : public IEditorInterface
{
public:
    SceneViewportInterface( Framebuffer& viewport )
        : mNewSize( 640, 640 ),
          mViewport( viewport )
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
        if ( mNewSize != mViewport.GetSize() )
            mViewport.Resize( mNewSize );
    }

    void OnDraw() override
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 } );
        ImGui::Begin( Name().data(), &mOpen, ImGuiWindowFlags_NoCollapse |
                                             ImGuiWindowFlags_NoTitleBar );
        {
            vec2 viewportSize = mViewport.GetSize();
            ImVec2 imguiViewportSize = ImGui::GetContentRegionAvail();

            u64 textureId = mViewport.GetColorAttachmentRendererID();
            ImVec2 textureSize = ImVec2( (float)mViewport.GetWidth(),
                                         (float)mViewport.GetHeight() );
            ImGui::Image( (ImTextureID)textureId, textureSize,
                          ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
            mNewSize = { imguiViewportSize.x, imguiViewportSize.y };

            //if ( ImGui::IsItemHovered() )
            //    args.mSceneCamera->OnUpdate( dt );
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    uvec2 mNewSize;
    Framebuffer& mViewport;
};

}
