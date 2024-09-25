
#include <sol/sol.hpp>
#include "engine/scene/scene.hpp"
#include "engine/scripting/script.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/scripting/bindings/glm_lua_bindings.hpp"
#include "engine/scripting/bindings/scene_lua_bindings.hpp"
#include "engine/scripting/bindings/window_input_bindings.hpp"
#include "engine/scripting/bindings/loader_lua_bindings.hpp"

#include <print>

namespace bubble
{
constexpr string_view ON_UPDATE_FUNC = "OnUpdate"sv;

ScriptingEngine::ScriptingEngine( WindowInput& input,
                                  Loader& loader,
                                  Scene& scene )
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

    CreateWindowInputBindings( *mLua );
    CreateLoaderBidnings( *mLua );
    CreateSceneBindings( *mLua );
    
    // Engine bindings
    mLua->set( "bLoader", &loader );
    mLua->set( "bScene", &scene );

    mLua->set(
        "bIsKeyCliked", [&]( int key ) { 
                if ( key <= (int)MouseKey::LAST )
                    return input.IsKeyCliked( MouseKey( key ) );
                return input.IsKeyCliked( KeyboardKey( key ) );
        } 
    );

    mLua->set(
        "bIsKeyPressed", [&]( int key ) {
            if ( key <= (int)MouseKey::LAST )
                return input.IsKeyPressed( MouseKey( key ) );
            return input.IsKeyPressed( KeyboardKey( key ) );
        }
    );
}


ScriptingEngine::~ScriptingEngine()
{

}


void ScriptingEngine::OnUpdate( Ref<Script>& script )
{
    if ( not script )
        return;

    auto scriptFunctionsIter = mCache.find( script->mName );
    if ( scriptFunctionsIter == mCache.end() )
    {
        mLua->set( ON_UPDATE_FUNC, sol::nil );
        RunScript( script );
        auto onUpdate = mLua->get<sol::function>( ON_UPDATE_FUNC );

        auto newName = script->mName + string( ON_UPDATE_FUNC );
        mLua->set( newName, onUpdate );
        auto newOnUpdate = mLua->get<sol::function>( newName );

        scriptFunctionsIter = mCache.emplace(
            script->mName,
            FunctionCache{ .mOnUpdate=newOnUpdate }
        ).first;
    }
    auto& scriptFunctions = scriptFunctionsIter->second;
    if ( scriptFunctions.mOnUpdate )
        scriptFunctions.mOnUpdate();
}

void ScriptingEngine::RunScript( const Script& script )
{
    mLua->safe_script( script.mCode );
}

void ScriptingEngine::RunScript( const Ref<Script>& script )
{
    RunScript( *script );
}

}