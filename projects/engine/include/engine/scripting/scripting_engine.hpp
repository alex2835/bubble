#pragma once
#include "engine/utils/pointers.hpp"

namespace bubble
{
class sol::state;

class ScriptingEngine
{
public:
    ScriptingEngine();
    void RunScript( string_view script );

private:
    Scope<sol::state> mLua;
};

}