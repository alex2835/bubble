#pragma once
#include "engine/engine.hpp"
#include <variant>

namespace bubble
{
enum class ProjectNodeType
{
    ProjectMain,
    Folder,
    Camera,
    GameObject,
    MainCharacter
};

struct ProjectNode
{
    ProjectNodeType mType = ProjectNodeType::Folder;
    vector<ProjectNode> mChildren;

    //std::variant<Entity> mState;
};

}