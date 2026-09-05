#include "engine/pch/pch.hpp"
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/script.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/types/set.hpp"
#include "engine/types/any.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scripting/bindings/spawn_lua_bindings.hpp"
#include "engine/scripting/bindings/for_each_lua_bindings.hpp"
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
    mLoader = &loader;
    CreateLoaderBindings( loader, *mLua );
}

// Held by reference: Engine declares mTimer before mProject, so the timer
// outlives the Lua state that closes over it.
void ScriptingEngine::BindTimer( Timer& timer )
{
    mLua->set( "time", [&timer]() { return timer.GetElapsed().Seconds(); } );
}

void ScriptingEngine::BindScene( Scene& scene, PhysicsEngine& physicsEngine )
{
    // The scene bindings take resources by path - add_model("cube.obj") - so
    // the loader has to be bound first.
    if ( not mLoader )
        throw std::runtime_error( "BindScene: BindLoader must be called first" );

    // Scene first: it builds the Component enum that for_each_entity's ids come
    // from, and registers the component usertypes spawn hands back.
    CreateSceneBindings( scene, *mLoader, physicsEngine, *mLua );
    CreateSpawnBindings( scene, *mLoader, physicsEngine, *mLua );
    CreateForEachBindings( scene, *mLua );
    CreatePhysicsBindings( physicsEngine, *mLua );
}

static void RunScriptIn( sol::state& lua, const Script& script )
{
    // Without a chunk name every diagnostic reads [string "..."]:<line> with no
    // way to tell which script failed. Script::mChunkName is built at load time;
    // this fallback only covers a Script assembled by hand.
    static const string unnamedChunk = "@<unnamed script>";
    const string& chunkName = script.mChunkName.empty() ? unnamedChunk : script.mChunkName;

    const auto result = lua.safe_script( script.mCode, sol::script_pass_on_error, chunkName );
    if ( not result.valid() )
    {
        const sol::error err = result;
        throw std::runtime_error( std::format( "Script '{}' failed to load.\n  {}\n  {}",
                                               script.mName, err.what(), script.mPath.string() ) );
    }
}

ScriptCallbacks ExtractScriptCallbacks( sol::state& lua, const Ref<Script>& script )
{
    if ( not script )
        throw std::runtime_error( "ExtractScriptCallbacks: script is null (it failed to load)" );

    // Every script shares one Lua state, so both names have to be cleared
    // first. Otherwise a script with no on_start silently inherits the one left
    // behind by whichever script was extracted before it.
    lua.set( ON_START_FUNC, sol::nil );
    lua.set( ON_UPDATE_FUNC, sol::nil );
    RunScriptIn( lua, *script );

    if ( not lua.get<sol::object>( ON_UPDATE_FUNC ).is<sol::function>() )
        throw std::runtime_error(
            std::format( "Script '{}' defines no global function {}( entity, state, dt ).\n  {}",
                         script->mName, ON_UPDATE_FUNC, script->mPath.string() ) );

    ScriptCallbacks callbacks;
    callbacks.mOnUpdate = lua.get<sol::protected_function>( ON_UPDATE_FUNC );

    // Optional, a script that only needs per frame work does not have to
    // declare an empty one.
    if ( lua.get<sol::object>( ON_START_FUNC ).is<sol::function>() )
        callbacks.mOnStart = lua.get<sol::protected_function>( ON_START_FUNC );

    return callbacks;
}

void CallScriptOnStart( const sol::protected_function& onStart,
                        const Ref<Script>& script,
                        recs::Entity entity,
                        const Any& state )
{
    if ( not onStart )
        return;

    sol::protected_function_result result = onStart( entity, state );
    if ( result.valid() )
        return;

    const sol::error err = result;
    const string name = script ? script->mName : string( "<unknown>" );
    const string scriptPath = script ? script->mPath.string() : string( "<no path>" );
    throw std::runtime_error( std::format( "Script '{}' failed in on_start on entity {}.\n  {}\n  {}",
                                           name, (u64)entity, err.what(), scriptPath ) );
}

ScriptCallbacks ScriptingEngine::ExtractCallbacks( const Ref<Script>& script )
{
    return ExtractScriptCallbacks( *mLua, script );
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