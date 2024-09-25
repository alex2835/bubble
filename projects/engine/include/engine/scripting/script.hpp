#pragma once
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/types/pointer.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class ScriptingEngine;

struct Script
{
    path mPath;
    string mName;
    string mCode;
};

}