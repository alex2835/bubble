#pragma once
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
struct Script
{
    ~Script();

    path mPath;
    string mName;
    string mCode;
    Ref<sol::function> mOnUpdate;
};

}