#pragma once
#include <map>
#include <vector>
#include <string>
#include "imgui.h"
#include "engine/utils/types.hpp"
#include "engine/utils/ieditor_interface.hpp"

namespace hr
{
class HotReloader;
}

namespace bubble
{
class EditorInterfaceLoader
{
public:
    EditorInterfaceLoader( ImGuiContext* context );
    ~EditorInterfaceLoader();

    void AddInterface( Ref<IEditorInterface> interface );
    void LoadInterfaces();

    void OnUpdate( DeltaTime dt );
    void OnDraw( Engine& engine );

private:
    ImGuiContext* mImGuiContext;
    Ref<hr::HotReloader> mHotReloader;
    map<string_view, Ref<IEditorInterface>> mInterfaces;
};
}
