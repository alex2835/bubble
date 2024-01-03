#pragma once
#include <map>
#include <string>
#include "engine/utils/pointers.hpp"
#include "ieditor_interface.hpp"

namespace bubble
{
class EditorInterfaces
{
public:

private:
    std::map<std::string, Ref<IEditorInterface>> mInterfaces;
};
}
