#pragma once
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "utils/imexp.hpp"
#include "window/window.hpp"
#include "window/event.hpp"

namespace bubble
{
class BUBBLE_ENGINE_EXPORT ImGuiManager
{
public:
    ImGuiManager( const Window& window );
    ~ImGuiManager();

    void Begin();
    void End();
    void OnEvent( const Event& event );

private:
    void ImGuiMultiViewports();

    const Window& mWindow;
    ImGuiContext* mContext;
};
}