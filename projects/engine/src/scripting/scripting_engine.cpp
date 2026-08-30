#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/script.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/set.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scripting/bindings/physics_lua_bindings.hpp"
#include "engine/window/window.hpp"
#include "engine/scripting/bindings/window_input_bindings.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"
#include "engine/scripting/bindings/free_function_lua_bindings.hpp"
#include "glm_lua_bindings.hpp"
#include <sol/sol.hpp>


namespace bubble
{
constexpr string_view ON_START_FUNC = "on_start"sv;
constexpr string_view ON_UPDATE_FUNC = "on_update"sv;

ScriptingEngine::ScriptingEngine()
    : mLua( CreateScope<sol::state>() )
{
    mLua->open_libraries( sol::lib::base,
                          sol::lib::table,
                          sol::lib::string,
                          sol::lib::os,
                          sol::lib::utf8,
                          sol::lib::io,
                          sol::lib::math );

    // Geometry
    CreateVec2Bindings( *mLua );
    CreateVec3Bindings( *mLua );
    CreateVec4Bindings( *mLua );
    CreateMat2Bindings( *mLua );
    CreateMat3Bindings( *mLua );
    CreateMat4Bindings( *mLua );
    CreateMathFreeFunctionsBindings( *mLua );

    // General purpose free function
    CreateFreeFunctionBindings( *mLua );
}

ScriptingEngine::~ScriptingEngine()
{

}

void ScriptingEngine::BindWindow( Window& window )
{
    CreateWindowBindings( window, *mLua );
}

void ScriptingEngine::BindInput( WindowInput& input )
{
    CreateWindowInputBindings( input, *mLua );
}

void ScriptingEngine::BindLoader( Loader& loader )
{
    CreateLoaderBindings( loader, *mLua );
}

void ScriptingEngine::BindScene( Scene& scene, PhysicsEngine& physicsEngine )
{
    CreateSceneBindings( scene, physicsEngine, *mLua );
    CreatePhysicsBindings( physicsEngine, *mLua );
}

void ScriptingEngine::ExtractCallbacks( sol::protected_function& onStart,
                                       sol::protected_function& onUpdate,
                                       const Ref<Script>& script )
{
    // Every script shares one Lua state, so both names have to be cleared
    // first. Otherwise a script with no on_start silently inherits the one left
    // behind by whichever script was extracted before it.
    mLua->set( ON_START_FUNC, sol::nil );
    mLua->set( ON_UPDATE_FUNC, sol::nil );
    RunScript( script );

    if ( not mLua->get<sol::object>( ON_UPDATE_FUNC ).is<sol::function>() )
        throw std::runtime_error(
            std::format( "Script '{}' defines no global function {}( entity, state, dt ).\n  {}",
                         script->mName, ON_UPDATE_FUNC, script->mPath.string() ) );

    onUpdate = mLua->get<sol::protected_function>( ON_UPDATE_FUNC );

    // Optional, a script that only needs per frame work does not have to
    // declare an empty one.
    if ( mLua->get<sol::object>( ON_START_FUNC ).is<sol::function>() )
        onStart = mLua->get<sol::protected_function>( ON_START_FUNC );
    else
        onStart = sol::protected_function();
}

void ScriptingEngine::RunScript( const Script& script )
{
    // Without a chunk name every diagnostic reads [string "..."]:<line> with no
    // way to tell which script failed. Script::mChunkName is built at load time;
    // this fallback only covers a Script assembled by hand.
    static const string unnamedChunk = "@<unnamed script>";
    const string& chunkName = script.mChunkName.empty() ? unnamedChunk : script.mChunkName;

    const auto result = mLua->safe_script( script.mCode, sol::script_pass_on_error, chunkName );
    if ( not result.valid() )
    {
        const sol::error err = result;
        throw std::runtime_error( std::format( "Script '{}' failed to load.\n  {}\n  {}",
                                               script.mName, err.what(), script.mPath.string() ) );
    }
}

void ScriptingEngine::RunScript( const Ref<Script>& script )
{
    if ( not script )
        throw std::runtime_error( "RunScript: script is null (it failed to load)" );
    RunScript( *script );
}

Table ScriptingEngine::CreateTable()
{
    return mLua->create_table();
}


void ScriptingEngine::SetCurrentState()
{
    Any::set_lua_state( mLua->lua_state() );
}

}