#pragma once
#include <array>

namespace bubble
{
// Which editor windows are open. A window's close button, the Windows menu,
// the window.show operator and the editor settings all go through these.
struct ShownWindows
{
    bool mEntities = true;
    bool mViewport = true;
    bool mProject = true;
    bool mConsole = true;
    bool mAnimationGraph = false;
};

// Every window by its menu label and the id window.show and the settings
// file know it by, in menu order.
struct WindowToggle
{
    const char* mLabel;
    const char* mId;
    bool ShownWindows::* mShown;
};

inline constexpr std::array<WindowToggle, 5> cWindowToggles = { {
    { "Entities", "entities", &ShownWindows::mEntities },
    { "Viewport", "viewport", &ShownWindows::mViewport },
    { "Project", "project", &ShownWindows::mProject },
    { "Console", "console", &ShownWindows::mConsole },
    { "Animation Graph", "animation_graph", &ShownWindows::mAnimationGraph },
} };

}
