#pragma once
#include "interface/ieditor_interface.hpp"

namespace bubble
{
class EditorViewportInterface : public IEditorInterface
{
public:
    EditorViewportInterface( Framebuffer& viewport )
        : mViewport( viewport )
    {

    }

    ~EditorViewportInterface() override
    {

    }

    string_view Name() override
    {
        return "Viewport";
    }

    void OnInit() override
    {
        mNewSize = { 640, 640 };
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
                          ImVec2( 1, 1 ), ImVec2( 0, 0 ) );
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
