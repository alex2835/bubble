#pragma once
#include "engine/types/string.hpp"
#include "engine/scene/scene.hpp"
#include <variant>

namespace bubble
{
enum class ProjectNodeType
{
    Level,
    Folder,
    MainCharacter,
    GameObject,
    Camera,
    Script,
};

struct ProjectNode
{
    ProjectNodeType mType = ProjectNodeType::Folder;
    vector<ProjectNode> mChildren;

    std::variant<Entity, string> mState;
};

}