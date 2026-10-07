# Bubble

A small 3D engine rebuilt from the old one (`E:\projects\bubble0.7`, kept
until parity) a stage at a time. Decisions live in the architecture doc,
"Bubble: архитектура движка":
https://claude.ai/code/artifact/cfe42e9b-bcd3-4e0b-bed7-338f80567534 -
read the section you touch before changing a design, and update the doc when
a decision changes. It also holds the stage plan: a feature waits for its
stage, however ready it looks in the old code.

## Build and test

Windows, from PowerShell (vcvars puts VS 18's clang-cl on the path):

```
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && cmake --preset windows-debug && cmake --build --preset windows-debug'
out\build\windows-debug\bin\bubble_tests.exe
```

- Presets: `windows-debug`, `windows-release`, `windows-profile` (Tracy),
  `linux` (clang 20 + libc++), `web` (Emscripten). Linux and web are checked
  by CI only.
- Clang everywhere, C++23, warnings are errors. CI's clang is 20, older than
  the local 22: a warning flag newer than 20 goes behind
  `check_cxx_compiler_flag`.
- Run release too when touching anything with lifetimes or the Luau stack.
- Format what you change with VS's clang-format before finishing:
  `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" -i <files>`
  (only `projects/` and `deps/luaubind/`, never vendored deps).

## Layout

```
deps/                         vendored, each with its license; listed in THIRD_PARTY_NOTICES.md
deps/luaubind/                our own small sol-like C++ binding for Luau; depends on Luau only
projects/engine/include/bubble/
    types/                    aliases, Handle/SlotMap, OptRef
    core/                     log, profile, utf8, AssetPath, process
    assets/                   AssetId, AssetRegistry (entries, AssetRef<T>, reload events)
    scripts/                  lua.hpp (luaubind names in bubble), ScriptAsset, ScriptRuntime
projects/engine/src/...       same tree
projects/tests/               doctest; folders follow the engine's, deps/ tests deps (luaubind too)
```

## Style

`STYLE.md` is the rule book - read it before writing code. The essentials:

- **Errors.** Nothing there and that is normal: `opt` / `OptRef`. Can fail
  and someone should hear why: `expected<T, E>` with a message a person can
  act on (what is wrong, what there is instead). Cannot happen if the code is
  right: `logic_error`, never caught to carry on; the engine's own
  costly invariants: `BUBBLE_ASSERT`, debug only. Context is added once per
  level on the way up; the code that stops an error logs it, once, and does
  not return it again. No error codes, bool + out-param or null for "failed".
- **Names.** Types and functions `PascalCase`, members `mName`, statics
  `sName`, constants `cName`, files `snake_case`. Everything a user sees -
  components, fields, Luau API, operators, files - is `snake_case`.
- **Formatting.** Four spaces, braces on their own line, spaces inside
  parentheses: `Foo( a, b )`. `and`, `or`, `not`. 120 columns.
- **No `std::`.** The standard library comes through `bubble/types`
  (`string`, `vector`, `hmap`, `opt`, `expected`, `unexpected`, `function`,
  `format`, `OsPath`, `Ref`, `u32`...); a missing name is added there as a
  `using`. Left as `std::`: `move`, `forward`, `hash` specializations, the C
  library at a C boundary.
- **Pointers.** Owning: `Scope`/`Ref`; a shared asset: `AssetRef<T>`. A link to something that outlives you:
  `T&`. A link to something that can go away: `Handle` from a `SlotMap`.
  Maybe-missing results and parameters: `OptRef<T>`, never a member. Raw
  pointers only at a C API boundary inside the low layers.
- **`Id` vs `Handle`.** `...Id` is saved (`uid`, `AssetId`); `...Handle` is a
  run-time link with a generation, never written to a file. Variables follow
  the type: `handle`, not `id`.
- **Comments** say why, in plain sentences, at the density of the code around
  them. No commented-out code.
- **Strings** are UTF-8 in `string`; identifiers ASCII; never build an
  `OsPath` from a `string` (use `PathFromUtf8`).
- **No globals** besides the log and the profiler. **No migration code.**
  **Profiling** through `bubble/core/profile.hpp` macros only.
- **Tests**: doctest, named as sentences, in the folder that mirrors the
  code; error messages checked by their text; Luau tests check the stack is
  balanced (`lua_gettop( L ) == 0`).

## Working rules

- Commit and push only when asked.
- Scripting work: use the `bubble-scripting` skill.
