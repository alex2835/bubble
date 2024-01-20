#pragma once
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class Project
{
public:
    const string& Name();
    const path& Root();

private:
    string mName;
    path mRoot;
};

}
