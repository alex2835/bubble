#pragma once
#include <sol/forward.hpp>
#include <sol/function.hpp>
// SetVar subscripts the state; a forward declaration is not enough for a
// template body clang checks before instantiation.
#include <sol/state.hpp>
#include <recs/entity.hpp>
#include "engine/window/input.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/script.hpp"

namespace bubble
{
struct Loader;
class Scene;
class PhysicsEngine;
class Window;

// A script's entry points. on_update is required; on_start is empty when the
// script does not define one.
struct ScriptCallbacks
{
    sol::protected_function mOnStart;
    sol::protected_function mOnUpdate;
};

// Run a script chunk and pull its entry points out of the state. Free rather
// than a ScriptingEngine member because the scene bindings attach scripts too
// (add_script, spawn{ script = ... }) and hold only the sol::state.
ScriptCallbacks ExtractScriptCallbacks( sol::state& lua, const Ref<Script>& script );

// Run one script's on_start, reporting which script and entity failed. Shared
// by engine startup and by a script attached at runtime.
// onStart and script are taken BY VALUE on purpose. Every caller reaches them
// through a ScriptComponent, which lives in a component pool, and on_start is
// free to spawn or to call add_script - either of which pushes into that same
// pool, reallocating it and freeing the callable while it is executing. The
// copies are a second reference to the same Lua function and a second count on
// the script, and they own their lifetime for the duration of the call.
void CallScriptOnStart( sol::protected_function onStart,
                        Ref<Script> script,
                        recs::Entity entity,
                        const Any& state );


class ScriptingEngine
{
public:
    ScriptingEngine();
    ScriptingEngine( const ScriptingEngine& ) = delete;
    ScriptingEngine& operator=( const ScriptingEngine& ) = delete;
    ScriptingEngine( ScriptingEngine&& ) = default;
    ScriptingEngine& operator=( ScriptingEngine&& ) = default;
    ~ScriptingEngine();

    static sol::state& GlobalState();


    void BindWindow( Window& window );
    void BindInput( WindowInput& input );
    void BindLoader( Loader& loader );
    void BindTimer( Timer& timer );
    void BindScene( Scene& scene, PhysicsEngine& physicsEngine );

    void RunScript( const Script& script );
    void RunScript( const Ref<Script>& script );
    // Runs the script chunk once and pulls both entry points out of it. Both
    // in one call because running the chunk twice would repeat whatever it
    // does at load time.
    ScriptCallbacks ExtractCallbacks( const Ref<Script>& script );
    Table CreateTable();
    void SetCurrentState();

    template <typename T>
    void SetVar( string_view name, T&& var )
    {
        (*mLua)[name] = std::forward<T>( var );
    }

private:
    static sol::state* sGlobalLua;

    // Remembered from BindLoader so BindScene can hand it to the scene
    // bindings, which load models and shaders by path.
    Loader* mLoader = nullptr;

public:
    Scope<sol::state> mLua;
};

}