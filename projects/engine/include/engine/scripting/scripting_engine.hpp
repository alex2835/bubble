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
class Window;

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
    void BindScene( Scene& scene, PhysicsEngine& physicsEngine );

    void RunScript( const Script& script );
    void RunScript( const Ref<Script>& script );
    // Runs the script chunk once and pulls both entry points out of it.
    // on_update is required, on_start is optional and comes back empty when the
    // script does not define one. Both in one call because running the chunk
    // twice would repeat whatever it does at load time.
    void ExtractCallbacks( sol::protected_function& onStart,
                           sol::protected_function& onUpdate,
                           const Ref<Script>& script );
    Table CreateTable();
    void SetCurrentState();

    template <typename T>
    void SetVar( string_view name, T&& var )
    {
        (*mLua)[name] = std::forward<T>( var );
    }

private:
    static sol::state* sGlobalLua;

public:
    Scope<sol::state> mLua;
};

}