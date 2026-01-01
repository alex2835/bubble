#pragma once
#include <sol/forward.hpp>
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class ScriptingEngine;

struct Script
{
    path mPath;
    string mName;
    string mCode;

    inline void Swap( Script& other ) noexcept
    {
        std::swap( mPath, other.mPath );
        std::swap( mName, other.mName );
        std::swap( mCode, other.mCode );
    }
};

}