---
name: bubble-scripting
description: Work on Bubble's Luau scripting - luaubind (deps/luaubind, our small sol-like binding - LuaState, LuaTable, LuaFunction, LuaThread), ScriptRuntime, their tests, and game scripts (.luau). Covers the script model (props, self, callbacks, libraries, coroutines, events, hot reload), luaubind's API, how C++ binds functions and types, and the lifetime rules that keep scripts from crashing the engine. Use when touching deps/luaubind, projects/engine/*/bubble/scripts, projects/tests/scripts or projects/tests/deps/luaubind, or writing a game script.
---

# Bubble scripting (Luau)

The design is in the architecture doc, section «Скрипты»
(https://claude.ai/code/artifact/cfe42e9b-bcd3-4e0b-bed7-338f80567534).
This skill is how the code implements it and what to keep in mind.

## Files

| file | what it is |
| --- | --- |
**luaubind** (`deps/luaubind`, namespace `luaubind`) is our own library: it
depends on Luau and the standard library only, never on the engine, and is
tested in `projects/tests/deps/luaubind`. The engine pulls its names into
`bubble` through `bubble/scripts/lua.hpp`.

| file | what it is |
| --- | --- |
| `luaubind/state` | **`LuaState`, the VM and the only door to it**: `NewTable`, `Globals`, `Value`, `Function`, `Load`, `Call`, `NewThread`/`Resume`/`Yieldable`, `CallerEnvironment`, `Encode`/`Decode`/`DeepCopy`, `Seal`, atoms, `FindType`, the print handler. Tracks the active thread |
| `luaubind/value` | `LuaValue` (any value held from C++; copy = same value; nil when empty; `Kind`, `Truthy`, `As<T>`, `Describe`, `==`), `LuaTable` (`t["k"] = v`, `t["a"]["b"]`, `Get`/`RawGet`/`Set`/`RawSet`, `Pairs`, `Length`, `Append`, `SetMetatable`, `Freeze`, `Clone`), `LuaFunction` (`fn( args... )`), `LuaThread` (`Resume( args... )`) |
| `luaubind/stack` | `LuaTraits<T>` (`cName`, `Is`, `Push`, `Check`), `LuaError`, `LuaRest`, `LuaYield`, `LuaPushFunction` - the binding machinery |
| `luaubind/call` | `CompileScript`, `ScriptError`; raw `LoadScript`, `PCall`, `DescribeValue` under `LuaState` |
| `luaubind/type` | `LuaType`/`LuaTypeBuilder<T>`: C++ structs as tagged userdata, fields and methods by atom |
| `luaubind/data` | raw `LuaEncode`/`LuaDecode`/`LuaDeepCopy` under `LuaState::Encode`/`Decode`/`DeepCopy` |
| `scripts/lua` (engine) | the luaubind names in `bubble`, and `PrintToLog` - the print handler engine states use |
| `scripts/script_asset` | `ScriptAsset` (bytecode) and `RegisterScriptImporter`: `.luau` → bytecode in the asset registry; compiles on the spot for now |
| `scripts/script_runtime` | `ScriptRuntime`: runs script assets in its VM; entity scripts (one per path), instances, libraries, coroutines, events, hot reload on registry changes; hands out `ScriptHandle`/`ScriptInstanceHandle`. Written on luaubind's types only, no `lua_*`. One header; the code in `src/scripts/runtime/`: `runtime` (construction, globals, aliases), `files` (environment, `RunFile`/`RunScript`/`RunLibrary`, passport), `require`, `scripts` (`Load`/`Unload`), `reload` (`Changed`, `Rerun`), `instances`, `events`, `tasks` (coroutines, `Tick`), `text.hpp` (message helpers) |

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

locals { l_reload = 0 }         -- each instance's own state; the scene cannot set it

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
  An override with an unknown key or another kind than the default is an
  error. `entity` is reserved. Props must be data (`Encode`-able).
- **`locals { ... }`** is the instance's own state - a reload timer, a
  counter: deep-copied into `self` next to the props, flat as they are, but
  not in `Props`, not settable by an override (an error saying so) and not
  saved. A name is a prop or a local, never both. A prefix like `l_` is a
  convention, not a rule. A file-level `local` is the opposite: one for the
  whole file, shared by every instance.
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
- **Hot reload** (from the registry): new functions for every instance,
  `self` kept, new props and locals added, instances switched back on; a
  failing file leaves the old one in place.

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

- A library has no `props`, no `locals` and no callbacks and **returns**
  what it shares.
- **Behaviour over time belongs to an instance.** A library's function
  called from an instance runs for it (`start`/`on` inside are that
  instance's). The top of any file belongs to no one, even when a callback's
  `require` runs it (`RunFile` sets an empty `CurrentScope`), so `on`/`start`
  there are errors. Something world-wide is an entity with a script.
- **Names:** an entity's file is a *script* (`ScriptHandle`, `RunScript`),
  a required one a *library* (`RunLibrary`); both run their top through
  `RunFile`. Luau and Roblox call a required file a module, so nothing of
  ours is called that.
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
  imports the file into the same entry and tells listeners; the runtime then
  re-runs a changed script, or drops a changed library's value (it runs again
  at its next require) and re-runs every script that required it, directly or
  through other libraries. A file that fails keeps its old code; a file that
  does not compile never leaves the registry.
- **The graph is in the passport**: `require` records the library in
  `mRequiresOf[env]` of the run that called it, so what is known is what the
  code running now required. A failed run's environment is dropped with
  everything it recorded - no bookkeeping to undo.

## C++ recipes

```cpp
AssetRegistry assets( AssetRegistry::FromDirectory( projectRoot ) );   // one per process
RegisterScriptImporter( assets );
LuaState lua( PrintToLog );                            // outlives everything below
ScriptRuntime runtime( lua, assets, { "on_start", "on_update" } );
lua.Globals()["raycast"] = [&]( Vec3 from, Vec3 to ) { return Hit( from, to ); };   // a lambda is a function
LuaTypeBuilder<Light>( lua, "light" ).Field( "brightness", &Light::mBrightness ).Method( "dim", &Dim );
lua.Seal();                                            // globals and types first, then seal

runtime.SetAlias( "lib", "scripts/lib" );
// The world loads what it needs first and holds the handles.
auto held = { assets.Load<ScriptAsset>( *AssetPath::From( "scripts/player.luau" ) ),
              assets.Load<ScriptAsset>( *AssetPath::From( "scripts/lib/inventory.luau" ) ) };
auto script = runtime.Load( "scripts/player.luau" );   // one script per path
auto player = runtime.Create( *script, "/player" );
runtime.Call( *player, OnUpdate, dt );                 // self first, then the args
runtime.Emit( "door_opened", "north" );

LuaTable self = runtime.Self( *player );
f64 speed = self["speed"].As<f64>().value_or( 0 );    // reads are explicit, never implicit
self["stats"]["hp"] = 10;                              // nested, writes through
for ( const auto& [key, value] : self.Pairs() ) … ;    // a snapshot: the loop may change the table
```

- **Order:** `LuaState` → `ScriptRuntime` (adds globals) → engine bindings
  and types → `Seal()` → load assets → `Load`. `Load` refuses an unsealed
  state and a script the registry has not loaded; the runtime refuses a
  sealed state.
- **Who owns what:** the registry owns script assets (bytecode); the runtime
  holds handles to what it ran and owns only its VM's state. It never reads
  a file.
- **Bound functions** (`table["name"] = lambda`, or `lua.Function( name, fn )`) take parameters
  `LuaTraits` knows (`LuaValue` for anything) and a last `LuaRest` for `...`.
  They return nothing, one value, a `std::tuple`, or `LuaYield` to suspend
  the calling coroutine. `throw LuaError( "..." )` is a script error at the
  calling line ("player.luau:12: ..."). A new parameter type means a new
  `LuaTraits` specialization.
- **Types:** the userdata holds a `T` by value. For a component that `T`
  must be a handle (entity + component), never a pointer into a pool.
  Register types before scripts load so their names get atoms early.

## Rules that keep it from crashing

1. **Raw `lua_*` lives only in luaubind.** The engine - the runtime,
   modules, the editor - uses `LuaState`, `LuaTable`, `LuaFunction`,
   `LuaThread` and never sees the stack. A missing operation is added to
   luaubind, with a test, rather than written raw at the call site. luaubind
   never includes engine headers. It keeps the stack balanced on every path
   (`StackRestore`); tests assert `lua_gettop( lua.L() ) == 0`. Writing to a
   frozen table or through nil is a `std::logic_error`, not a script error.
2. **Never hold a reference into the runtime across a call into Luau.**
   Scripts create and destroy instances (themselves too) mid-call. Look the
   handle up before the call and again after it, as `ScriptRuntime::Call`
   does. Coroutines are found again by their thread, not by index.
3. **The active thread is LuaState's business.** Bound functions and
   `Resume` switch it, so C++ called from a coroutine works on the
   coroutine's stack by itself. Raw code in the binding layer works on
   `lua.Active()`, not on `lua.L()`.
4. **Script code runs only through `LuaState::Call`/`Resume`** (or the
   runtime's `Call`/`Emit`/`Tick`), which are protected. `LuaError` is for
   bound functions; anywhere else an error is an `expected`.
5. **A `LuaValue` must die before its `LuaState`.** Member order: state
   first. A value captured by a bound function lives as long as the
   function: per-file data goes in weak-keyed tables, not in closures that
   hold their own environment and so never go.
6. **No `Any`.** What goes to a file is `Value` (stage 1); working state stays
   in Luau as `LuaValue`s (`Encode`, `DeepCopy` for saves and snapshots).

## Luau facts that bite

- One number type, a double: integers are exact to 2^53; `LuaTraits` for
  integral types refuses fractions and out-of-range values.
- `vector` is three floats, no allocation; `LuaVectorLike` maps any `x, y, z`
  float struct (glm::vec3) to it.
- `#s` is bytes; characters are `utf8.len`/`utf8.codes`.
- Chunk names: `Load` prefixes `@` so errors read `player.luau:12:`; `=name`
  for something that is not a file.
- Per-file data - the passport: path, props, locals, library or not, the
  libraries required - is kept in weak-keyed tables by the environment of
  one run of the file; `props`, `locals` and `require` find their file with
  `CallerEnvironment()`.
- Luau is built as C++: errors are exceptions, destructors in bindings run.

## Tests

luaubind is tested in `projects/tests/deps/luaubind/` with its own
`helpers.hpp` (`Bytecode`, `RunLua`, `Evaluate`, `Balanced`) and no engine
headers: `state_test.cpp` is the API as users see it, `binding_test.cpp` the
traits, bound functions and types, `data_test.cpp` the codec.
The runtime is tested in `projects/tests/scripts/`: `script_helpers.hpp` has
`Bytecode` and `LogWatch` (assert what was logged); `script_fixture.hpp` has
the `Scripts` fixture - project files in memory behind a registry,
`record( text )` and `destroy()` bound for observing order and lifetimes,
`Load`/`AddLibrary` (write + load the asset), `Change` (write + registry
reload, as an edit on disk), `Evaluate`, `Field`/`Number`/`Set` on self.
Runtime tests are in `script_runtime_test.cpp`, libraries in
`script_require_test.cpp`.
Every new behaviour gets a test that would fail without it; error messages are
checked by their text, because the text is the feature.
