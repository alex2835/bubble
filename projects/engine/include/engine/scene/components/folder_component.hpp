#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
// Marks an entity that is there to hold others: a folder in the Entities
// tree. It has a transform like anything else - identity unless moved - so
// moving a folder moves what is in it; that makes it a group as much as a
// folder. Nothing else about it is special.
struct FolderComponent
{
    static int ID() { return static_cast<int>( ComponentID::Folder ); }
    static string_view Name() { return "folder"sv; }

    // Fields for engine/reflection: saved, shown, set by path and bound to Lua.
    static void Reflect();
};

}
