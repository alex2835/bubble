#include "bubble/editor/world_picture.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/render/render_scene.hpp"
#include <imgui.h>

namespace bubble
{
WorldPicture::WorldPicture( Gpu& gpu, Renderer& renderer ) : mGpu( gpu ), mRenderer( renderer )
{
}

void WorldPicture::Show( EditorCamera& camera )
{
    const ImVec2 area = ImGui::GetContentRegionAvail();
    if ( area.x < 1 or area.y < 1 )
        return;
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    const ivec2 pixels( static_cast<i32>( area.x * scale.x + 0.5f ), static_cast<i32>( area.y * scale.y + 0.5f ) );
    if ( not mPicture or mPicture.Size() != pixels )
        mPicture =
            GpuTexture( mGpu, GpuTextureOptions{ .mSize = pixels,
                                                 .mMips = 1,
                                                 .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                                 .mUsage = WGPUTextureUsage_RenderAttachment |
                                                           WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc,
                                                 .mLabel = "world picture" } );
    // Under a button that takes every mouse button, so a drag that starts on
    // it stays its own wherever the mouse goes.
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton( "picture", area,
                            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                ImGuiButtonFlags_MouseButtonMiddle );
    ImGui::GetWindowDrawList()->AddImage( ImGuiRenderer::TextureId( mPicture.View() ), corner,
                                          ImVec2( corner.x + area.x, corner.y + area.y ) );
    const ImGuiIO& io = ImGui::GetIO();
    const vec2 delta( io.MouseDelta.x * scale.x, io.MouseDelta.y * scale.y );
    if ( ImGui::IsItemActive() and ImGui::IsMouseDown( ImGuiMouseButton_Right ) )
        camera.Orbit( delta );
    else if ( ImGui::IsItemActive() and ImGui::IsMouseDown( ImGuiMouseButton_Middle ) )
        camera.Pan( delta, static_cast<f32>( pixels.y ) );
    if ( ImGui::IsItemHovered() and io.MouseWheel != 0 )
        camera.Zoom( io.MouseWheel );
}

void WorldPicture::Draw( World& world, const EditorCamera& camera )
{
    if ( not mPicture )
        return;
    mRenderer.Draw( mView, world.Get<RenderScene>(), camera.ToRenderCamera(),
                    RenderTarget{ .mView = mPicture.View(), .mSize = mPicture.Size(), .mFormat = mPicture.Format() } );
}
}
