#pragma once
#include <sol/forward.hpp>
#include "engine/window/input.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/any.hpp"
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
    ScriptingEngine& operator=( const ScriptingEngine& ) = default;
    ScriptingEngine& operator=( ScriptingEngine&& ) = default;
    ~ScriptingEngine();

    void BindInput( WindowInput& input );
    void BindLoader( Loader& loader );
    void BindScene( Scene& scene, PhysicsEngine& physicsEngine );

    void RunScript( const Script& script );
    void RunScript( const Ref<Script>& script );
    void ExtractOnUpdate( sol::protected_function& func, const Ref<Script>& script );
    Table CreateTable();

    template <typename T>
    void SetVar( string_view name, T& var )
    {
        (*mLua)[name] = var;
    }

public:
    Scope<sol::state> mLua;
};

}