#pragma once
#include "bubble/editor/editor_camera.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// A world's picture in a panel, only looked at: drawn by the renderer into a
// texture the panel's size in pixels, the mouse over it turning the camera
// with the right button, sliding it with the middle one, bringing it closer
// with the wheel. What a material's window shows its preview through.
class WorldPicture
{
public:
    WorldPicture( Gpu& gpu, Renderer& renderer );
    WorldPicture( const WorldPicture& ) = delete;
    WorldPicture& operator=( const WorldPicture& ) = delete;

    // Its widgets, inside an ImGui window, as large as what is left of it:
    // the picture last drawn - made again here when the size changed, before
    // ImGui's lists name it -, `camera` moved by the mouse.
    void Show( EditorCamera& camera );
    // `world` drawn into the picture, as `camera` sees it now: after the
    // widgets, before ImGui's lists are drawn.
    void Draw( World& world, const EditorCamera& camera );

    const GpuTexture& Picture() const { return mPicture; }

private:
    Gpu& mGpu;
    Renderer& mRenderer;
    RenderView mView;
    GpuTexture mPicture;
};
}
