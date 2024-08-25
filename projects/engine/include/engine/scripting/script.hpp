#pragma once
#include "engine/types/string.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class Script
{
public:
    Script( const path& scriptPath )
        : mPath( scriptPath ),
          mScript( filesystem::readFile( scriptPath ) )
    {}

    string_view GetCode() const { return mScript; }

private:
    path mPath;
    string mScript;
};

}