---
name: bubble-scripting
description: Work on Bubble's Luau scripting - the binding layer in bubble/scripts/lua, ScriptRuntime, their tests, and game scripts (.luau). Covers the script model (props, self, callbacks, coroutines, events, hot reload), how C++ binds functions and types, the Luau stack rules, and the lifetime rules that keep scripts from crashing the engine. Use when touching projects/engine/*/bubble/scripts, projects/tests/scripts, or writing a game script.
---

# Bubble scripting (Luau)

The design is in the architecture doc, section «Скрипты»
(https://claude.ai/code/artifact/cfe42e9b-bcd3-4e0b-bed7-338f80567534).
This skill is how the code implements it and what to keep in mind.

## Files

| file | what it is |
| --- | --- |
| `scripts/lua/lua_state` | `LuaState`: one VM per World or test. Libraries open, `print` → log, `Seal()` makes globals read-only, CodeGen on desktop, name atoms, registered `LuaType`s |
| `scripts/lua/lua_ref` | `LuaRef`: the only way C++ keeps a Luau value alive. Move-only; `Push( L )` brings it back |
| `scripts/lua/lua_stack` | `LuaTraits<T>` (`cName`, `Is`, `Push`, `Check`), `LuaPush`/`LuaCheck`, `LuaPushFunction`/`LuaSetFunction` |
| `scripts/lua/lua_call` | `CompileScript`, `LoadScript`, `PCall` (error + traceback as `ScriptError`), `DescribeValue` |
| `scripts/lua/lua_type` | `LuaType`/`LuaTypeBuilder<T>`: engine types as tagged userdata, fields and methods by atom |
| `scripts/lua/lua_data` | `LuaEncode`/`LuaDecode` (data ↔ bytes), `LuaDeepCopy` |
| `scripts/script_asset` | `ScriptAsset` (bytecode) and `RegisterScriptImporter`: `.luau` → bytecode in the asset registry; compiles on the spot for now |
| `scripts/script_runtime` | `ScriptRuntime`: runs script assets in its VM; modules (one per path), instances, libraries, coroutines, events, hot reload on registry changes; hands out `ScriptModuleHandle`/`ScriptInstanceHandle` |

Not there yet (stage 7): the World module that owns a runtime and a
runtime component per entity, `self.entity`, `Value` ↔ Luau and the
`asset`/`entity`/`color`/`list` constructors, props and requires read from
the AST on import, reading `.luaurc` (luau_config) into `SetAlias`,
`bubble.d.luau`, the `interrupt` guard against endless loops.

## The script model

```lua
props {
    speed       = 40,
    jump_height = prop( 10, { min = 0, max = 30 } ),   -- hints are for the inspector
    patrol      = { vector.create( 0, 0, 0 ) },
}

local TIME_TO_APEX = 0.5        -- file-level: shared by every instance
hits = 0                        -- a global of this file, declared at the top

function on_start( self )
    self.rise_gravity = 2 * self.jump_height / TIME_TO_APEX ^ 2
    on( "door_opened", function( self, door ) print( door ) end )
    start( function()
        wait( 1 )
        wait_until( function() return self.ready end )
    end )
end

function on_update( self, dt ) end
```

- **A file runs once per world** in an environment of its own over the
  sealed globals. After it ran, assigning a new global is an error; globals
  it declared stay assignable. Functions are shared by every instance.
- **`self`** is a deep copy of the props with the instance's overrides laid
  on - flat, `self.speed`, no `self.props`. After that a prop is plain state.
  An override with an unknown key or another type than the default is an
  error. `entity` is reserved. Props must be data (`LuaEncode`-able).
- **Callbacks** are the names given to `ScriptRuntime`'s constructor, called
  by index. A missing one is not called. A function named `on_*` that is not
  a callback logs a warning.
- **Errors** log label + message + traceback and switch that instance off
  (its coroutines dropped) until the file is reloaded. Other instances go on.
- **Coroutines**: `start( fn, ... )` runs to the first `wait` at once;
  `Tick( dt )` resumes them. `wait` outside `start` is an error.
- **Events**: `on( name, fn )` subscribes the running instance,
  `emit( name, ... )` / `ScriptRuntime::Emit` call subscribers in
  subscription order with `( self, ... )`. Gone with the instance.
- **Hot reload** (`Reload`): new functions for every instance, `self` kept,
  new props added, instances switched back on; a failing file leaves the old
  one in place.

## Libraries

```lua
-- scripts/lib/inventory.luau
local inventory = {}
function inventory.add( items, kind, count ) items[kind] = ( items[kind] or 0 ) + count end
return inventory

-- scripts/player.luau
local inventory = require( "@lib/inventory" )   -- alias from .luaurc
local tuning = require( "./player_tuning" )     -- next to this file
```

- A library has no `props` and no callbacks and **returns** what it shares.
- Paths follow Luau's own require so luau-lsp agrees: `./x`, `../x` from the
  requiring file, `@alias/x` through `SetAlias`; no extension; a bare path is
  an error. Use string literals - import will read them from the AST.
- **require never loads**: it takes the library from the registry, which the
  world filled before its scripts ran; a library not loaded there is an error
  naming the resolved path.
- Runs at its first require, once per world; its value and file-level state
  are shared by every entity of the world. Cycles are errors showing the
  chain. An error inside a library fails the requiring file with both
  places in the message.
- **Hot reload comes from the registry**: `AssetRegistry::Reload( path )`
  imports the file into the same slot and tells listeners; the runtime then
  re-runs a changed script, or drops a changed library's value (it runs again
  at its next require) and re-runs every script that required it, directly or
  through other libraries (`mRequires` is the graph). A file that fails keeps
  its old code; a file that does not compile never leaves the registry.

## C++ recipes

```cpp
AssetRegistry assets( AssetRegistry::FromDirectory( projectRoot ) );   // one per process
RegisterScriptImporter( assets );
LuaState state;                                        // outlives everything below
ScriptRuntime runtime( state, assets, { "on_start", "on_update" } );
LuaPushFunction( state.L(), "raycast", [&]( Vec3 from, Vec3 to ) { return Hit( from, to ); } );
lua_setglobal( state.L(), "raycast" );
LuaTypeBuilder<Light>( state, "light" ).Field( "brightness", &Light::mBrightness ).Method( "dim", &Dim );
state.Seal();                                          // globals and types first, then seal

runtime.SetAlias( "lib", "scripts/lib" );
// The world loads what it needs first and holds the handles.
auto held = { assets.Load<ScriptAsset>( *AssetPath::From( "scripts/player.luau" ) ),
              assets.Load<ScriptAsset>( *AssetPath::From( "scripts/lib/inventory.luau" ) ) };
auto module = runtime.Load( "scripts/player.luau" );   // one module per path
auto player = runtime.Create( *module, "/player" );
lua_pushnumber( state.L(), dt );
runtime.Call( *player, OnUpdate, 1 );                  // args on the stack, popped
```

- **Order:** `LuaState` → `ScriptRuntime` (adds globals) → engine bindings
  and types → `Seal()` → load assets → `Load`. `Load` refuses an unsealed
  state and a script the registry has not loaded; the runtime refuses a
  sealed state.
- **Who owns what:** the registry owns script assets (bytecode); the runtime
  holds handles to what it ran and owns only its VM's state. It never reads
  a file.
- **Bound functions** take parameters `LuaTraits` knows, plus `lua_State*`
  (handed the running thread, takes no argument). Return nothing, one value
  or a `std::tuple`. Throwing a `std::exception` is a script error with its
  `what()`. A new parameter type means a new `LuaTraits` specialization.
- **Types:** the userdata holds a `T` by value. For a component that `T`
  must be a handle (entity + component), never a pointer into a pool.
  Register types before scripts load so their names get atoms early.

## Rules that keep it from crashing

1. **Never hold a reference into the runtime across a call into Luau.**
   Scripts create and destroy instances (themselves too) mid-call. Look the
   handle up before the call and again after it, as `ScriptRuntime::Call` does.
2. **Use the `L` you were given.** A function called from a coroutine gets
   the coroutine's thread; its arguments are on that stack, not on
   `runtime.L()`.
3. **Keep the stack balanced.** Every function leaves the stack as it found
   it, error paths included (`lua_settop( L, top )`). Tests assert
   `lua_gettop( L ) == 0`.
4. **Raise Luau errors only inside protected code.** `luaL_error` throws a
   C++ exception through to the nearest `PCall`/`lua_resume`; from plain C++
   it would unwind into the engine. Call script code only through `PCall`,
   `ScriptRuntime::Call` or `lua_resume`.
5. **A `LuaRef` must die before its `LuaState`.** Member order: state first.
6. **No `Any`.** What goes to a file is `Value` (stage 1); working state stays
   in Luau and is handled on the stack (`LuaEncode`, `LuaDeepCopy`).

## Luau facts that bite

- One number type, a double: integers are exact to 2^53; `LuaTraits` for
  integral types refuses fractions and out-of-range values.
- `vector` is three floats, no allocation; `LuaVectorLike` maps any `x, y, z`
  float struct (glm::vec3) to it.
- `#s` is bytes; characters are `utf8.len`/`utf8.codes`.
- `lua_pushcfunction( L, fn, debugname )` takes a name; it is a macro, so no
  commas inside a lambda argument.
- Chunk names: `LoadScript` prefixes `@` so errors read `player.luau:12:`;
  `=name` for something that is not a file.
- Luau is built as C++: errors are exceptions, destructors in bindings run.

## Tests

`projects/tests/scripts/` (runtime) and `scripts/lua/` (binding layer);
`scripts/script_helpers.hpp` has `RunLua`, `Bytecode` and `LogWatch` (assert
what was logged). `scripts/script_fixture.hpp` has the `Scripts` fixture:
project files in memory behind a registry, `record( text )` and `destroy()`
bound for observing order and lifetimes, `Load`/`AddLibrary` (write + load
the asset) and `Change` (write + registry reload, as an edit on disk). Runtime tests are in `script_runtime_test.cpp`,
library tests in `script_require_test.cpp`.
Every new behaviour gets a test that would fail without it; error messages are
checked by their text, because the text is the feature.
