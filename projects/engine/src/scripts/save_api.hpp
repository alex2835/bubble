#pragma once
// save.write, save.read, save.info and save.list: a world's scripts and
// the game's save slots.
#include "bubble/scripts/lua.hpp"
#include "bubble/scripts/script_saves.hpp"

namespace bubble
{
class World;

// Into `lua` before it is sealed. No slots: every call is an error saying
// the game keeps none.
void RegisterSaves( LuaState& lua, World& world, Ref<SaveSlots> slots, string version );
}
