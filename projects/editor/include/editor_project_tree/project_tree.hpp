#pragma once
#include "engine.hpp"
#include <variant>

namespace bubble
{
enum class ProjectNodeType
{
    Folder,
    Camera,
    GameObject,
    MainCharacter
};

struct ProjectNode
{
    ProjectNode mType = ProjectNodeType::Folder;
    vector<ProjectNode> mChildren;

    std::variant<Ref<Camera>, Entity> mState;
};

}