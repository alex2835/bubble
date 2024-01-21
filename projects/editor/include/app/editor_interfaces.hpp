#pragma once
#include <map>
#include <vector>
#include <string>
#include "engine/utils/types.hpp"
#include "interface/ieditor_interface.hpp"

namespace hr
{
class HotReloader;
}

namespace bubble
{
class EditorInterfaces
{
public:
    EditorInterfaces();
    ~EditorInterfaces();

    void AddInterface( Ref<IEditorInterface> interface );
    void LoadInterfaces();

    void OnUpdate( DeltaTime dt );
    void OnDraw();

private:
    Ref<hr::HotReloader> mHotReloader;
    map<string_view, Ref<IEditorInterface>> mInterfaces;
};
}
