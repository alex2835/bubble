#pragma once
#include <imgui.h>
#include "engine/utils/types.hpp"

namespace hr
{
class HotReloader;
}

namespace bubble
{
class IEditorInterface;
struct EditorState;

class EditorInterfaceHotReloader
{
public:
    EditorInterfaceHotReloader( EditorState& editorState );
    ~EditorInterfaceHotReloader();

    void AddInterface( Ref<IEditorInterface> );
    void LoadInterfaces();

    void OnUpdate( DeltaTime dt );
    void OnDraw( DeltaTime dt );

private:
    EditorState& mEditorState;
    ImGuiContext* mImGuiContext;

    Ref<hr::HotReloader> mHotReloader;
    map<string_view, Ref<IEditorInterface>> mInterfaces;
};
}
