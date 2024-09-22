#pragma once
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
struct Script
{
    path mPath;
    string mName;
    string mCode;
};

}