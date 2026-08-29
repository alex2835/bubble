---
name: bubble-scripting
description: Write, review, or debug Lua gameplay scripts for the Bubble engine. Covers the OnUpdate script contract, the Entity/component Lua API, and the object-lifetime rules that make scripts crash or corrupt memory. Use when touching .lua game scripts, ScriptComponent, StateComponent, or anything under projects/engine/src/scripting/bindings.
---

# Bubble engine — Lua scripting

**Before writing or reviewing a script, read `docs/scripting.md`.** It is the
complete API surface — every global, method, field and enum member the engine
registers (156 names), verified against the bindings by
`tools/check_lua_api_docs.py`. This file does *not* repeat it, on purpose: two
copies of an API listing drift, and the reference is the one that is checked.

What follows is only the part a listing cannot express — the script contract and
the lifetime rules that make scripts crash.

Naming: the Lua API is `snake_case` for globals, methods, fields and enum
members. Type names stay PascalCase (`Entity`, `Transform`, `RayHitResult`),
and glm keeps its GLSL spelling (`vec3`, `mat4`).

## Script contract

A script is a Lua file that defines a global `OnUpdate`:

```lua
function OnUpdate( entity, state )
    -- entity: the Entity this ScriptComponent is attached to
    -- state:  this entity's StateComponent, a plain Lua table
end
```

Three things are required for it to ever run, and none of them produce a useful
error if missing:

1. The entity needs **both** a `ScriptComponent` and a `StateComponent`. The
   engine calls scripts from `ForEach<StateComponent, ScriptComponent>`, so an
   entity with only a `ScriptComponent` is silently skipped.
2. The `ScriptComponent` must have a script asset assigned, or `OnStart` throws
   `Entity:{} Script not set`.
3. `OnUpdate` must be a **global**. It is extracted by name once at startup, not
   looked up per frame. Defining it as `local` means it is never found.

`state` persists across frames and is serialised into the project file. It is
the only place to keep per-entity script data — locals in `OnUpdate` do not
survive the call, and globals are shared by every script in the process.

## Lifetime rules

These are the ones that cause crashes rather than wrong behaviour.

### R1 — Do not keep anything `for_each_entity` hands you

```lua
for_each_entity( { Component.transform, Component.tag }, function( entity, comps )
    comps[Component.transform].position.x = 0   -- fine, inside the callback
end )
```

The table is reused for every entity, and the components in it are pointers
into engine memory. Both the table and its contents are valid **only for the
duration of that one callback call**. Copy out what you need:

```lua
local results = {}
for_each_entity( { Component.tag }, function( entity, comps )
    table.insert( results, { entity = entity, name = comps[Component.tag].name } )
end )
```

`Component.state` is the exception — it is passed by value, so it stays valid.

### R2 — Do not add or remove entities while iterating

Mutating the scene inside a `for_each_entity` callback invalidates the iteration
and every component reference the engine is holding, including the ones passed
to the callback. This applies to `OnUpdate` too: the engine calls scripts from
inside its own iteration, so **`create_entity` / `add_*_component` / `remove_entity`
called directly from `OnUpdate` are not safe today.**

Queue the work and apply it outside the iteration:

```lua
function OnUpdate( entity, state )
    state.pendingSpawns = state.pendingSpawns or {}
    if shouldSpawn then
        table.insert( state.pendingSpawns, position )   -- do not spawn here
    end
end
```

This is a known engine limitation, not a style preference. If you are working on
the engine rather than a script, see the note at the end of this file.

### R3 — `remove_entity` is one-shot

Removing an entity that is already gone raises an error. Entity handles held in
`state` across frames go stale. There is no `is_valid` binding yet, so track
removal in your own state rather than retrying.

### R4 — Component references do not survive a structural change

`local t = entity:get_transform()` is a reference into a component pool. Adding
or removing *any* component of that type, on any entity, can move it. Fetch it
again after any such change rather than holding it across one.

## API rules

- `for_each_entity` takes **at most 10** component ids, and each must be a
  `Component.*` value. Anything else raises an error naming the problem.
- Passing an empty table `{}` iterates nothing. It does not iterate everything.
- Adding a `RigidBody` or `CharacterController` component to an entity that
  already has one replaces it and re-registers it with the physics world. This
  is handled, but it is not free — do not do it per frame.
- There are no `add_script_component` / `get_script` / `has_script_component`
  bindings, and no `remove_*_component` bindings at all. Scripts cannot attach
  scripts or detach components.
- Input keys are one flat namespace: `is_key_pressed` takes either a
  `KeyboardKey.*` or a `MouseKey.*` value and dispatches on the numeric range.

## Source of truth

The bindings are the specification. When `docs/scripting.md` and the source
disagree, the source wins and the doc is stale:

| Surface | File |
|---|---|
| Entity methods, `create_entity`, `remove_entity`, `for_each_entity`, `Component` enum | `projects/engine/src/scripting/bindings/scene_lua_bindings.cpp` |
| Per-component usertypes | `projects/engine/src/scene/components/*_component.cpp` → `CreateLuaBinding` |
| Raycasts, `RigidBody:set_mass` | `projects/engine/src/scripting/bindings/physics_lua_bindings.cpp` |
| Keyboard/mouse, key enums | `projects/engine/src/scripting/bindings/window_input_bindings.cpp` |
| Asset loading | `projects/engine/src/scripting/bindings/loader_lua_bindings.cpp` |
| `vec2/3/4`, `mat2/3/4`, math helpers | `deps/glm_lua_bindings/src/` |
| `dt`, `global_state`, active camera | `projects/engine/src/engine.cpp` (`OnStart` / `OnUpdate`) |

Run `python tools/check_lua_api_docs.py` to check the doc against the bindings.

## If you are changing the engine, not writing a script

R2 is the open one. The engine invokes `OnUpdate` from inside
`ForEach<StateComponent, ScriptComponent>` in `Engine::OnUpdate`, which is what
makes script-driven mutation unsafe. The candidate fixes are: snapshot the
entity list and run scripts after the iteration ends; add a deferred mutation
queue flushed at a frame boundary; or have `for_each_entity` pass entities rather
than component pointers. None is implemented.
