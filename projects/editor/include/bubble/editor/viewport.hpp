#pragma once
#include "bubble/editor/editor_camera.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
// The panel that shows a world - the active document's - through the
// editor's camera: a picture the panel's size in pixels, the mouse over it
// driving the camera - the right button turns it, the middle one slides it,
// the wheel brings it closer.
class Viewport
{
public:
    Viewport( Gpu& gpu, Renderer& renderer );
    Viewport( const Viewport& ) = delete;
    Viewport& operator=( const Viewport& ) = delete;

    // Its widgets, inside an ImGui window: the picture last drawn, and the
    // camera as the mouse moves it. The picture is made again here when the
    // panel's size changed, before ImGui's lists name it.
    void Show();
    // `world` drawn into the picture, as the camera sees it now: after the
    // widgets, before ImGui's lists are drawn. The first world it draws
    // places the camera where the scene's first camera is, if it has one.
    void Draw( World& world );

    EditorCamera& GetCamera() { return mCamera; }
    const GpuTexture& Picture() const { return mPicture; }

private:
    Gpu& mGpu;
    Renderer& mRenderer;
    RenderView mView;
    GpuTexture mPicture;
    EditorCamera mCamera;
    bool mPlaced = false;
};
}
