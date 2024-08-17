
#include <sol/sol.hpp>
#include "engine/utils/types.hpp"
#include "engine/scripting/scripting_engine.hpp"
#include "engine/scripting/glm_bindings.hpp"

namespace bubble
{
ScriptingEngine::ScriptingEngine()
	: mLua( CreateScope<sol::state>() )
{
	mLua->open_libraries( sol::lib::base,
						  sol::lib::table,
						  sol::lib::string,
						  sol::lib::io,
						  sol::lib::math );

	bubble::CreateGLMBindings( *mLua );
}

void ScriptingEngine::RunScript( string_view script )
{
	mLua->safe_script( script );
}

}