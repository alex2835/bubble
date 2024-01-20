#pragma once
#include <map>
#include <vector>
#include <string>
#include "engine/utils/types.hpp"
#include "ieditor_interface.hpp"

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
    std::map<string, Ref<IEditorInterface>> mInterfaces;
};
}
