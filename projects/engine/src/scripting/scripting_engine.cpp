
#include <sol/sol.hpp>
#include "engine/loader/loader.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/script.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scripting/bindings/window_input_bindings.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"
#include "glm_lua_bindings.hpp"

namespace bubble
{
constexpr string_view ON_UPDATE_FUNC = "OnUpdate"sv;

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

    CreateVec2Bindings( *mLua );
    CreateVec3Bindings( *mLua );
    CreateVec4Bindings( *mLua );
    CreateMat2Bindings( *mLua );
    CreateMat3Bindings( *mLua );
    CreateMat4Bindings( *mLua );
    CreateMathFreeFunctionsBindings( *mLua );
}

ScriptingEngine::~ScriptingEngine()
{

}

void ScriptingEngine::BindInput( WindowInput& input )
{
    CreateWindowInputBindings( input, *mLua );
}

void ScriptingEngine::BindLoader( Loader& loader )
{
    CreateLoaderBidnings( loader, *mLua );
}

void ScriptingEngine::BindScene( Scene& scene, PhysicsEngine& physicsEngine )
{
    CreateSceneBindings( scene, physicsEngine, *mLua );
}

void ScriptingEngine::ExtractOnUpdate( sol::function& func, const Ref<Script>& script )
{
    mLua->set( ON_UPDATE_FUNC, sol::nil );
    RunScript( script );
    auto onUpdate = mLua->get<sol::function>( ON_UPDATE_FUNC );
    
    // Save function in lua state with script name to don't overlap with other
    auto newName = script->mName + string( ON_UPDATE_FUNC );
    mLua->set( newName, onUpdate );
    func = mLua->get<sol::function>( newName );
}

void ScriptingEngine::RunScript( const Script& script )
{
    mLua->script( script.mCode );
}

void ScriptingEngine::RunScript( const Ref<Script>& script )
{
    RunScript( *script );
}

}