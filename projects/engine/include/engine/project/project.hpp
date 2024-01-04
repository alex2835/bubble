#pragma once
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class Project
{
public:
    const std::string& Name();
    const std::path& Root();

private:
    std::string mName;
    std::path mRoot;
};

}
