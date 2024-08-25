#pragma once
#include <sol/forward.hpp>
#include "engine/types/pointer.hpp"

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