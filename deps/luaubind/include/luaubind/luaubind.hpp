#pragma once
// luaubind: a small binding of C++ to Luau, in the spirit of sol2 but for
// Luau and much smaller. Everything at once:
//   LuaState                      the VM, and the only door to it
//   LuaValue, LuaTable,           values held from C++; table["key"],
//   LuaFunction, LuaThread        fn( args... ), thread.Resume( args... )
//   LuaTypeBuilder<T>             C++ structs as Luau userdata types
//   CompileScript                 source to bytecode
//   LuaError, LuaRest, LuaYield   what bound functions throw, take, return
#include "luaubind/call.hpp"
#include "luaubind/data.hpp"
#include "luaubind/stack.hpp"
#include "luaubind/state.hpp"
#include "luaubind/type.hpp"
#include "luaubind/value.hpp"
