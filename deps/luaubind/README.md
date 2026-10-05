# luaubind

A small C++23 binding for [Luau](https://luau.org), in the spirit of
[sol2](https://github.com/ThePhD/sol2) but for Luau and much smaller. It
depends on Luau alone. Written for the Bubble engine; MIT, like it.

```cpp
#include <luaubind/luaubind.hpp>
using namespace luaubind;

LuaState lua;
LuaTable globals = lua.Globals();
globals["greet"] = []( string name ) { return "hello, " + name; };   // a lambda is a function
lua.Seal();                                                           // globals read-only from here

LuaTable env = lua.NewTable();
env["speed"] = 5;
f64 speed = env["speed"].As<f64>().value_or( 0 );                    // reads are explicit

auto chunk = lua.Load( "=demo", *CompileScript( "return greet( 'luau' )" ), globals );
auto result = ( *chunk )();                                           // expected<LuaValue, ScriptError>
```

What it has:

- **`LuaState`** - the VM and the only door to it. Values come back as
  `LuaValue`; calls come back as `expected`, with the error and its
  traceback. It tracks the running thread, so C++ called from a coroutine
  works on the coroutine's stack by itself.
- **`LuaTable`** - `table["key"] = value`, `table["a"]["b"]`,
  `Get`/`RawGet`/`Set`/`RawSet`, `Pairs()`, `Length`, `Append`,
  `SetMetatable`, `Freeze`, `Clone`.
- **`LuaFunction`** - `fn( args... )`, protected.
- **`LuaThread`** - `thread.Resume( args... )`: waiting, finished or failed.
- **Bound functions** - any lambda: arguments checked and converted
  (`LuaTraits`), a last `LuaRest` takes `...`, return values or a tuple,
  `LuaYield` to suspend the calling coroutine, `throw LuaError( "..." )`
  for an error at the calling script line.
- **`LuaTypeBuilder<T>`** - a C++ struct as a userdata type: fields and
  methods found by name atom, errors that list what the type has.
- **Data** - `Encode`/`Decode` (values to bytes and back, for saves) and
  `DeepCopy`.

What it leaves out on purpose: implicit conversions to C++ types (a read is
always `As<T>()`, an `opt`), exceptions out of calls (they are `expected`),
overloads and inheritance for userdata types.
