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

    // What Lua reports this script as in errors and tracebacks, with the '@'
    // prefix that marks it a file name. Built at load time: it is a property of
    // the script, and sol2 wants a std::string to hand to luaL_loadbufferx.
    string mChunkName;

    inline void Swap( Script& other ) noexcept
    {
        std::swap( mPath, other.mPath );
        std::swap( mName, other.mName );
        std::swap( mCode, other.mCode );
        std::swap( mChunkName, other.mChunkName );
    }
};

}