#pragma once
#include "bubble/types/string.hpp"
#include <luaubind/luaubind.hpp>

// The engine works with Luau through luaubind (deps/luaubind); these bring
// its names into bubble.
namespace bubble
{
using luaubind::CompileOptions;
using luaubind::CompileScript;
using luaubind::LuaError;
using luaubind::LuaFunction;
using luaubind::LuaKind;
using luaubind::LuaRest;
using luaubind::LuaResume;
using luaubind::LuaState;
using luaubind::LuaTable;
using luaubind::LuaThread;
using luaubind::LuaTraits;
using luaubind::LuaType;
using luaubind::LuaTypeBuilder;
using luaubind::LuaValue;
using luaubind::LuaYield;
using luaubind::ScriptError;

// What a LuaState of the engine gives as its print handler: script output
// into the log, as script messages.
void PrintToLog( string_view text );
}
