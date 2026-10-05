# Bubble code style

How the code is written, so that every file reads like the others. Where a
rule and the code around it disagree, follow the rule and fix the code.

## Errors

Three tools, chosen by what the caller is to do about it.

| situation | tool | example |
| --- | --- | --- |
| **Nothing there, and that is normal** - a lookup, an optional value | `opt<T>`, `OptRef<T>` | `Find( path )`, `value.As<int>()`, `SlotMap::Get` |
| **It can fail, and someone should hear why** - a file, input from a person or a script, a format | `expected<T, E>` | `AssetPath::From`, `CompileScript`, `ScriptRuntime::Load`, `AssetRegistry::Reload` |
| **It cannot happen if the code is right** - a caller broke the contract | exception (`std::logic_error`), every build | the wrong C++ type asked of a Luau type, a write to a frozen table, a runtime built after `Seal()` |
| **The engine's own invariant**, too costly to check always or on a hot path | `BUBBLE_ASSERT( condition, "message" )`, debug builds only | a sorted list still sorted, a slot index in range inside a tight loop |

- **`expected` carries a message a person can act on**: what is wrong, and
  what there is instead. `"no alias @libs (aliases: @lib)"`, not
  `"bad alias"`. The error type is `string`, or a struct when more travels
  with it (`ScriptError`: message and traceback).
- **Add context on the way up, once per level.** A caller that passes an
  error on says where it was: `"player.luau: " + error`. It does not repeat
  what the error already says.
- **The one who stops the error logs it.** Code that returns an error does not
  also log it; the code that handles it - shows it, switches an instance off,
  keeps the old version - logs it, once, and gives its caller at most a
  `bool` of how it went (`ScriptRuntime::Call`), not the error again.
- **An `expected` is never dropped silently.** When ignoring one is right,
  say so: `(void)CopyInto( ... ); // props are data; a failure would have
  failed the run`.
- **Exceptions and asserts are not for control flow.** Nothing catches a
  `logic_error` to carry on; a failed `BUBBLE_ASSERT` stops the program.
  Both are bugs to fix. The one exception inside the engine is
  luaubind's `LuaError`: a bound function throws it to raise a script error,
  and the binding turns it into one at once.
- **Not used:** error codes, `bool` plus an out-parameter, a null pointer for
  "failed", sentinel values (`-1`, empty string) for "failed". A function
  that can fail says so in its return type.
- Out of memory and the like are not handled: the process ends.

## Names

- Types and functions `PascalCase`; members `mName`, statics `sName`,
  constants `cName`; locals and parameters `camelCase`; files `snake_case`.
- Everything a user sees is `snake_case`: components, fields, Luau API,
  operators, files, names of new entities.
- `...Id` is an identity that is saved (`uid`, `AssetId`); `...Handle` is a
  run-time link with a generation, never saved. Variables follow the type.
- Names say what a thing is in the game's terms: `mRequires`, not
  `mDependencyMap2`. No abbreviations beyond the common ones (`dt`, `id`,
  `L` for a `lua_State*`).

## Formatting

`.clang-format` does it - run it on what you change; code it would change is
not done. It covers `projects/` and `deps/luaubind/`; vendored libraries keep
their own style. What it enforces:

- Four spaces; braces on their own line; spaces inside parentheses and
  brackets of calls: `Foo( a, b )`, `{ 1, 2 }`.
- Lines up to 120 columns.
- `and`, `or`, `not` instead of `&&`, `||`, `!`.
- One statement per line; a one-line body without braces is fine, two lines
  get braces.
- Early return over nesting: check, return, carry on.

## Types and ownership

- The aliases from `bubble/types`: `string`, `string_view`, `vector`,
  `array`, `hmap`, `str_hmap`, `hset`, `str_hset`, `opt`, `expected`,
  `Scope`, `Ref`, `i32`, `u32`, `f32`, `f64`... luaubind has its own small
  set in `luaubind/common.hpp`, the same names over the standard library.
- Owning: `Scope` or `Ref`. A link to what outlives you: `T&`. A link to
  what can go away: a `Handle` from a `SlotMap`. Maybe-missing results and
  parameters: `OptRef<T>`, never a member. Raw pointers only at a C API
  boundary inside the low layers (luaubind, platform, GPU, physics).
- Views (`span`, `string_view`) do not outlive the call that got them.
- `const` wherever a value does not change: locals, parameters by
  reference, member functions.
- `auto` when the type is on the same line or obvious (`auto handle =
  registry.Load<ScriptAsset>( path )`), the type written out otherwise.
- Strings are UTF-8 in `std::string`; identifiers ASCII; never a
  `std::filesystem::path` from `std::string` - `PathFromUtf8`.
- No globals besides the log and the profiler.

## Comments

- Say why, or what is not obvious - not what the next line plainly does.
- Plain sentences, at the density of the code around them. A header comment
  says what the thing is for and what it promises; the code says how.
- No commented-out code, no `TODO` without a stage or an owner in the
  architecture doc.

## Files and includes

- `include/bubble/<area>/<name>.hpp` with `src/<area>/<name>.cpp`; tests in
  `projects/tests/<area>/<name>_test.cpp`.
- `#pragma once`. The file's own header first in a `.cpp`, then project
  headers, then libraries, then the standard library, each group sorted.
- A header includes what it uses and nothing for its users' convenience.

## Tests

- doctest; a test case is named as a sentence about behaviour: "A stale
  handle finds nothing".
- Every behaviour gets a test that would fail without it. Error messages
  are checked by their text, because the text is the feature.
- Tests that touch Luau end with the stack where they found it:
  `lua_gettop( L ) == 0`.

## Not done

- No migration code: the engine has no projects yet; a changed format just
  changes.
- No DLLs, no C++ hot reload, no exceptions across module boundaries.
