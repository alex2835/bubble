#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/color.hpp"
#include "bubble/scene/entity.hpp"
#include "bubble/types/string.hpp"
#include <luaubind/luaubind.hpp>

// The engine works with Luau through luaubind (deps/luaubind); these bring
// its names into bubble.
namespace bubble
{
using luaubind::CompileOptions;
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
using luaubind::TryCompileScript;

// What a LuaState of the engine gives as its print handler: script output
// into the log, as script messages.
void PrintToLog( string_view text );
}

// The engine's value types cross into Luau as types of their own, by value;
// a world's scripts module registers them in its state.
template <>
struct luaubind::LuaUserType<bubble::Entity>
{
    static constexpr const char* cName = "entity";
};

template <>
struct luaubind::LuaUserType<bubble::Color>
{
    static constexpr const char* cName = "color";
};

template <>
struct luaubind::LuaUserType<bubble::AssetId>
{
    static constexpr const char* cName = "asset";
};
