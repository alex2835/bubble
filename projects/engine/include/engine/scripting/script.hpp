#pragma once
#include "engine/utils/types.hpp"
#include "engine/utils/filesystem.hpp"

namespace bubble
{
class Script
{
public:
    Script( const path& scriptPath )
        : mPath( scriptPath ),
          mScript( readFile( scriptPath ) )
    {}

    string_view GetCode() { return mScript; }

private:
    path mPaht;
    string mScript;
};

}