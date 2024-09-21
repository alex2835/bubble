
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
ScriptingEngine::ScriptingEngine( WindowInput& input,
                                  Loader& loader,
                                  Scene& scene )
	: mLua( CreateScope<sol::state>() )
{
	mLua->open_libraries( sol::lib::base,
						  sol::lib::table,
						  sol::lib::string,
						  sol::lib::io,
						  sol::lib::math );

    CreateVec2Bindings( *mLua );
    CreateVec3Bindings( *mLua );
    CreateVec4Bindings( *mLua );
    CreateMat2Bindings( *mLua );
    CreateMat3Bindings( *mLua );
    CreateMat4Bindings( *mLua );
    CreateMathFreeFunctionsBindings( *mLua );

    CreateSceneBindings( *mLua );
    
    CreateWindowInputBindings( *mLua );

    CreateLoaderBidnings( *mLua );
    
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


void ScriptingEngine::RunScript( const Script& script )
{
    mLua->safe_script( script.GetCode() );
}

}