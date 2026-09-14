#pragma once
#include "engine/editing/ui/inspector_context.hpp"
#include "engine/editing/commands/lua_value_command.hpp"

namespace bubble
{
// ImGui inspector for a component's Lua table: scalars inline, tables as
// trees with add/remove. Every edit is a SetLuaValueCommand on `ctx`'s
// history. `fixedKeys` keeps the set of keys as it is (a shader's uniforms
// are the shader's to decide) and only lets the values change.
void DrawLuaTable( InspectorContext& ctx, const LuaTableRoot& root, bool fixedKeys = false );

}
