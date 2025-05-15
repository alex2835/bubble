#pragma once
#include <sol/forward.hpp>
#include "engine/window/input.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/scripting/script.hpp"

namespace bubble
{
struct Loader;
class Scene;
class PhysicsEngine;

class ScriptingEngine
{
public:
    ScriptingEngine();
    ScriptingEngine( const ScriptingEngine& ) = delete;
    ScriptingEngine( ScriptingEngine&& ) = delete;
    ScriptingEngine& operator=( const ScriptingEngine& ) = delete;
    ScriptingEngine& operator=( ScriptingEngine&& ) = delete;
    ~ScriptingEngine();

    void BindInput( WindowInput& input );
    void BindLoader( Loader& loader );
    void BindScene( Scene& scene, PhysicsEngine& physicsEngine );

    void RunScript( const Script& script );
    void RunScript( const Ref<Script>& script );
    void ExtractOnUpdate( sol::function& func, const Ref<Script>& script );

private:
    Scope<sol::state> mLua;
};

}