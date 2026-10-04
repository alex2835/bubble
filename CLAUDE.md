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

## Layout

```
deps/                         vendored, each with its license; listed in THIRD_PARTY_NOTICES.md
projects/engine/include/bubble/
    types/                    aliases, Handle/SlotMap, OptRef
    core/                     log, profile, utf8, AssetPath, process
    assets/                   AssetId, AssetRegistry (slots, AssetHandle<T>, reload events)
    scripts/lua/              Luau binding layer
    scripts/                  ScriptRuntime
projects/engine/src/...       same tree
projects/tests/               doctest; folders follow the engine's, deps/ checks vendored libraries
```

## Conventions

- **Names.** Types and functions `PascalCase`, members `mName`, statics
  `sName`, constants `cName`, files `snake_case`. Everything a user sees -
  components, fields, Luau API, operators, files - is `snake_case`.
- **Style.** Four spaces, braces on their own line, spaces inside
  parentheses: `Foo( a, b )`. `and`, `or`, `not`. Use the aliases from
  `bubble/types` (`string`, `string_view`, `vector`, `hmap`, `str_hmap`,
  `hset`, `opt`, `expected`, `Scope`, `Ref`, `u32`, `f32`...).
- **Comments** say why, in plain sentences, at the density of the code around
  them. No commented-out code.
- **Errors** are `expected<T, string>` (or a richer error type) with a
  message a person can act on: what is wrong and what there is instead.
  Exceptions only for engine bugs (`std::logic_error`).
- **Pointers.** Owning: `Scope`/`Ref`. A link to something that outlives you:
  `T&`. A link to something that can go away: `Handle` from a `SlotMap`.
  Maybe-missing results and parameters: `OptRef<T>`, never a member. Raw
  pointers only at a C API boundary inside the low layers.
- **`Id` vs `Handle`.** `...Id` is an identity that is saved: an entity's
  `uid`, `AssetId` (GUID), a prefab node id. `...Handle` is a run-time link
  with a generation, never written to a file: `ScriptInstanceHandle`,
  `AssetHandle<T>`. Variables follow the type: `handle`, not `id`.
- **Strings** are UTF-8 in `std::string`; identifiers ASCII; never build a
  `std::filesystem::path` from `std::string` (use `PathFromUtf8`).
- **No globals** besides the log and the profiler.
- **No migration code**: the engine has no projects yet; a changed format
  just changes.
- **Profiling** through `bubble/core/profile.hpp` macros only.
- **Tests**: doctest, named as sentences ("A stale handle finds nothing"),
  in the folder that mirrors the code. Script tests check the Luau stack is
  balanced (`lua_gettop( L ) == 0`).

## Working rules

- Commit and push only when asked.
- Scripting work: use the `bubble-scripting` skill.
