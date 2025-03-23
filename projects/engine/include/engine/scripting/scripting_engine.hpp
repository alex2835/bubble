#pragma once
#include <sol/function.hpp>
#include "engine/window/input.hpp"
#include "engine/types/pointer.hpp"
#include "engine/types/map.hpp"
#include "engine/scripting/script.hpp"

namespace bubble
{
class Scene;
struct Loader;

class ScriptingEngine
{
public:
    ScriptingEngine();
    ScriptingEngine( const ScriptingEngine& ) = delete;
    ScriptingEngine( ScriptingEngine&& ) = delete;
    ScriptingEngine& operator=( const ScriptingEngine& ) = delete;
    ScriptingEngine& operator=( ScriptingEngine&& ) = delete;
    ~ScriptingEngine();

    void bindInput( WindowInput& input );
    void bindLoader( Loader& loader );
    void bindScene( Scene& scene );

    void RunScript( const Script& script );
    void RunScript( const Ref<Script>& script );
    sol::function ExtractOnUpdate( Ref<Script>& script );

private:
    Scope<sol::state> mLua;

    //struct FunctionCache
    //{
    //    sol::unsafe_function mOnUpdate;
    //};
    //str_hash_map<FunctionCache> mCache;
};

}