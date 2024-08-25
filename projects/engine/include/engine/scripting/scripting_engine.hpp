#pragma once
#include "engine/utils/pointers.hpp"
#include <sol/forward.hpp>

namespace bubble
{
class Scene;
class Script;

class ScriptingEngine
{
public:
    ScriptingEngine( Scene& scene );
    ~ScriptingEngine();

    void RunScript( const Script& script );

private:
    Scope<sol::state> mLua;
};

}