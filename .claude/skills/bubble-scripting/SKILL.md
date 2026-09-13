---
name: bubble-scripting
description: Write, review, or debug Lua gameplay scripts for the Bubble engine. Covers the on_update script contract, the Entity/component Lua API, and the object-lifetime rules that make scripts crash or corrupt memory. Use when touching .lua game scripts, ScriptComponent, StateComponent, or anything under projects/engine/src/scripting/bindings.
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

Component accessors carry **no `_component` suffix** and there is exactly one
name per component: `entity:add_state( t )`, `entity:get_camera()`,
`entity:has_rigid_body()`. `add_state_component` / `get_camera_component` and
friends no longer exist.

## Script contract

A script is a Lua file that defines a global `on_update`, and optionally a
global `on_start`:

```lua
function on_start( entity, state )       -- optional, once when the game starts
end

function on_update( entity, state, dt )
    -- entity: the Entity this ScriptComponent is attached to
    -- state:  this entity's StateComponent, a plain Lua table
    -- dt:     seconds since the last frame
end
```

`on_start` is the place for one time setup — capturing the cursor, seeding
`state`, choosing the active camera. When it runs depends on how the script got
there:

- **Attached in the editor**: once at startup, after every script has been
  extracted and every engine global is bound, before the first `on_update` and
  before the first physics step.
- **Attached at runtime** by `entity:add_script( path )` or
  `spawn{ script = ... }`: synchronously inside that call, before it returns.

Either way it may spawn and remove entities freely — both paths run it outside a
live scene walk, and the callable is copied before the call so that a nested
`add_script` cannot free it mid-execution. The entity's first `on_update` is the
next frame in both cases.

Three things are required for a script to ever run, and none of them produce a
useful error if missing:

1. The entity needs **both** a `ScriptComponent` and a `StateComponent`. The
   engine calls scripts from `ForEach<StateComponent, ScriptComponent>`, so an
   entity with only a `ScriptComponent` is silently skipped.
2. The `ScriptComponent` must have a script asset assigned, or `OnStart` throws
   `Entity:{} Script not set`.
3. `on_update` must be a **global**. It is extracted by name once at startup,
   not looked up per frame. Defining it as `local` means it is never found.
   The same goes for `on_start`, except that a missing one is not an error - it
   is simply never called, which looks identical to a `local` one.

`dt` is a parameter, not a global. All scripts share one Lua state with no
environment isolation, so anything you assign to a global name is visible to
every other script — keep per-entity data in `state`.

`state` persists across frames and is serialised into the project file. It is
the only place to keep per-entity script data — locals in `on_update` do not
survive the call, and globals are shared by every script in the process.

## Lifetime rules

These are the ones that cause crashes rather than wrong behaviour.

The one thing that separates these from the rest of the API: breaking them
corrupts memory rather than producing a wrong answer, and the damage usually
surfaces frames later somewhere unrelated.

The whole of it follows from one fact. **Components live in pooled arrays.**
`Pool::Push` reallocates and frees the old buffer; `Pool::Remove` compacts every
pool the entity touched and shifts everything after the hole. `spawn`, `add_*`
and `remove_entity` all do one or the other, and scripts are now free to call
them — so a reference taken on one line can be dangling on the next.

### R1 — Do not keep anything `for_each_entity` hands you

```lua
for_each_entity( { Component.transform, Component.tag }, function( entity, comps )
    comps.transform.position = vec3( 0, 0, 0 )   -- fine, inside the callback
end )
```

The callback table is keyed by the component's snake_case **name**, not by the
`Component.*` id used to select it. `comps[Component.transform]` is `nil`.

The table is reused for every entity and the component objects in it are raw
pointers into the pools. Both the table and those objects are valid **only for
the duration of that one callback call**. Fields read off them are copies and
may be kept:

```lua
local results = {}
for_each_entity( { Component.tag }, function( entity, comps )
    table.insert( results, { entity = entity, name = comps.tag.name } )
end )
```

`comps.state` is the exception — it is the entity's own state table, passed by
value, so it stays valid.

### R2 — Do not add or remove entities inside `for_each_entity`

`for_each_entity` walks the pools live, so mutating the scene inside the
callback invalidates the iteration and every pointer in `comps`. Queue and
apply after the loop:

```lua
local dead = {}
for_each_entity( { Component.tag }, function( entity, comps )
    if comps.tag.name == "dead" then table.insert( dead, entity ) end
end )
for _, e in ipairs( dead ) do remove_entity( e ) end
```

**`on_update` and `on_start` are not subject to this.** The engine snapshots the
scripted entities before calling any of them and looks each one up again as it
goes, so `spawn`, `create_entity`, `add_*` and `remove_entity` are safe to call
directly from either callback.

Two consequences:

- An entity created by a script runs its `on_start` immediately and its first
  `on_update` on the **next** frame — it is not in this frame's snapshot.
- An entity removed by an earlier script this frame is skipped when its turn
  comes. That is not an error.

### R3 — Entity handles go stale, and that is the only thing you may keep

`state` is the only place for per-entity script data, and an `Entity` is the
only piece of the scene that may go in it. Handles are safe to keep because ids
are never reused — a stale handle stays stale and can never come to mean a
different entity.

Test before use. `remove_entity` on something already gone still raises:

```lua
for i = #state.spawned, 1, -1 do
    if not state.spawned[i]:is_valid() then
        table.remove( state.spawned, i )
    end
end
```

### R4 — Keep fields, not components

```lua
state.t = entity:get_transform()             -- NO: pointer into a pool
state.p = entity:get_transform().position    -- yes: the field is a copy
```

Every `get_*` returns `T&`, which sol pushes as a pointer into the pool's
buffer. Re-fetch it each time rather than holding one across anything that can
add or remove a component — on any entity, not just this one.

**Every field read off a component is a copy** — primitives as Lua values,
`vec3`/`mat4` as fresh values — so all of them are safe to keep. The flip side
is that **mutating a sub-field of one does nothing**:

```lua
local t = entity:get_transform()
t.position.x = 5                                     -- SILENT NO-OP
t.position = vec3( 5, t.position.y, t.position.z )   -- assigns through
t.position = t.position + vec3( 0, dt, 0 )           -- fine
```

This is by design (`ValueProperty` in
`engine/scripting/lua_value_property.hpp`). The reference form was the single
easiest way to corrupt memory from a script — it read exactly like a value and
was safe until the next `spawn`. The copy that ignores `.x = 5` fails the first
time it runs. The `Entity` shorthands (`entity.position`, `.rotation`, `.scale`)
always worked this way, so a field now behaves the same regardless of the path
to it.

**Safe to keep in `state`:** entity handles · numbers, strings, tables · any
field read off a component · `Ref`s from `load_model` / `load_shader` /
`load_sound`.

**Never:** the component object itself from `entity:get_*()` · anything from a
`for_each_entity` `comps` table other than the fields you copied out.

## API rules

- `for_each_entity` takes **at most 10** component ids, and each must be a
  `Component.*` value. Anything else raises an error naming the problem.
- Passing an empty table `{}` iterates nothing. It does not iterate everything.
- Adding a `RigidBody` or `CharacterController` component to an entity that
  already has one replaces it and re-registers it with the physics world. This
  is handled, but it is not free — do not do it per frame.
- `entity:add_script( path )` exists and attaches a `StateComponent` too, then
  runs `on_start` immediately. There is no `get_script` / `has_script`, and no
  `remove_*` bindings at all — scripts cannot detach a component.
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
| Keyboard/mouse, key enums, cursor control | `projects/engine/src/scripting/bindings/window_input_bindings.cpp` |
| Asset loading | `projects/engine/src/scripting/bindings/loader_lua_bindings.cpp` |
| `vec2/3/4`, `mat2/3/4`, math helpers | `deps/glm_lua_bindings/src/` |
| `dt`, `global_state`, active camera, `on_start` / `on_update` dispatch | `projects/engine/src/engine.cpp` (`OnStart` / `OnUpdate`) |

Run `python tools/check_lua_api_docs.py` to check the doc against the bindings.

## If you are changing the engine, not writing a script

`Engine::OnUpdate` no longer walks the scene live to call scripts. It fills
`mScriptEntities` from `ForEach<StateComponent, ScriptComponent>` first, then
iterates that snapshot, re-checking `HasEntity`/`HasComponent` and re-fetching
both components per entity. The `sol::protected_function` is copied out before
the call, because the callable itself lives in the ScriptComponent pool and a
script that spawns something carrying a script would otherwise free it mid-call.

Any new per-entity callback that runs user code has to follow the same pattern.
A live `ForEach` is only safe when nothing it calls can touch the scene.

`for_each_entity` is the remaining gap. It hands raw component pointers into a
reused table (R1) and walks the pools live (R2). Fixing it means passing
entities rather than component pointers, so the script re-fetches through the
normal accessors — which costs a lookup per component and would change every
script that uses it.

R4 was narrowed by binding every `vec3`/`mat4` member through `ValueProperty`
(`engine/scripting/lua_value_property.hpp`) instead of a raw member pointer, so
fields come back as copies. Any new component field of usertype type must use
it — a bare `&T::mMember` for a `vec3` reintroduces the pool-pointer trap. What
remains is the component object itself: `get_*` returns `T&`, which is
unavoidable while scripts mutate components in place. Returning a handle that
re-resolves per access would close that too, at a lookup per access.
