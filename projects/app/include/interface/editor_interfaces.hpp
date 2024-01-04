#pragma once
#include <map>
#include <vector>
#include <string>
#include "engine/utils/pointers.hpp"
#include "ieditor_interface.hpp"

namespace bubble
{
class EditorInterfaces
{
public:
    EditorInterfaces();
    ~EditorInterfaces();

    void LoadInterfaces();

    void OnUpdate( DeltaTime dt );
    void OnDraw();

private:
    std::map<std::string, Ref<IEditorInterface>> mInterfaces;
};
}
