#pragma once
#include "luaubind/common.hpp"

struct lua_State;

namespace luaubind
{
// What went wrong in a script: the message ("player.luau:12: attempt to
// index nil") and the stack it was raised in, innermost first.
struct ScriptError
{
    string mMessage;
    string mTraceback;
};

struct CompileOptions
{
    // 1 keeps every line and local debuggable; 2 inlines, for shipped builds.
    int mOptimization = 1;
};

// Source to bytecode. The engine loads bytecode only - scripts are compiled
// on import and the runner ships without the compiler - but the editor and
// tests compile here too.
expected<string, string> CompileScript( string_view source, const CompileOptions& options = {} );

// Pushes the chunk as a function, or returns why it cannot be loaded. With
// `env` it runs in the table at that index instead of the globals. `chunk`
// names it in errors and tracebacks: "scripts/player.luau", or "=console"
// for something that is not a file.
expected<void, string> LoadScript( lua_State* L, string_view chunk, string_view bytecode, int env = 0 );

// Calls the function under `nargs` arguments, like lua_pcall. On success
// `nresults` results are left (LUA_MULTRET for all); on failure the
// function and its arguments are gone and the error says where it happened.
expected<void, ScriptError> PCall( lua_State* L, int nargs, int nresults );

// The value at `index` as a person reads it in an error: "nil", "42",
// "table", "vector(1, 2, 3)".
string DescribeValue( lua_State* L, int index );
}
