# Bubble — hierarchy and prefabs

## Hierarchy

The scene is one tree of entities. Every entity has a `HierarchyComponent`
and hangs under exactly one parent, up to a single **root** - the entity that
stands for the level (a folder named after the level file). The *Entities*
window draws this tree straight from the scene; there is no second tree.

An entity's `TransformComponent` is relative to its parent: moving, turning or
scaling the parent carries the child with it.

| Piece | Where | What |
|---|---|---|
| `HierarchyComponent` | `scene/components/hierarchy_component.hpp` | `mParent`, `mChildren` (in order). Both are saved. |
| `FolderComponent` | `scene/components/folder_component.hpp` | Marks a folder: an entity with a Tag and a Transform and nothing else to do. The root is one too. |
| `Scene::Root()` | `scene/scene.hpp` | The root entity. Saved as `"Root"` in the scene. |
| world cache | `TransformComponent::World()`, `WorldMatrix()` | Where the entity is in the world. Filled by `UpdateWorldTransforms`. Not saved. |
| functions | `engine/scene/hierarchy.hpp` | `ParentOf`, `ChildrenOf`, `IndexInParent`, `IsAncestor`, `Subtree`, `SetParent`, `AttachChild`, `DetachFromParent`, `CreateChildEntity`, `CopySubtree`, `ComputeWorldMatrix`, `SetWorldTransform`, `UpdateWorldTransforms` |

**Folders are groups.** A folder is an entity with a transform, so moving a
folder moves what is in it. A new folder sits at the origin, so what goes in
it does not move.

**Local vs world.** The transform's own fields - what the inspector, the
gizmo's result, files and scripts' `position`/`rotation`/`scale` read and
write - are local. Everything that places something in the world reads the
cache: drawing, picking, physics, lights, audio, cameras, IK targets, the
skeleton overlay. The engine refreshes the cache after physics and again
after the scripts; the editor refreshes it every frame.

- **Parent** by dragging an entity onto another in *Entities* (or cut and
  paste into it), or with `scene.move{ entity, parent, index }`. The moved
  entity keeps its place in the world; its local transform is worked out
  against the new parent. Undo puts the local transform back. The root cannot
  be moved, deleted or cut, and nothing goes under itself.
- **Selecting** an entity selects that entity only; what hangs under it
  follows.
- **The gizmo** moves the entity in the world (Shift for world axes) and
  writes the local transform back.
- New entities land where they were asked to in the world, whatever their
  parent.

## Names and paths

An entity's name is its Tag's name. **Among one parent's children no two
names are the same**: a name that is taken gets a number - `chair`, `chair2`,
`chair3` (a taken `chair2` becomes `chair3`). That happens wherever an entity
arrives under a parent or is renamed: made, pasted, moved, instantiated,
renamed in the tree or the inspector, spawned, `add_tag`, `set_parent`,
`entity.name = ...`, and on load (a file edited by hand may repeat a name;
the later ones get numbers). A name cannot hold `/`, and cannot be empty, `.`,
`..` or `~`. (Setting `get_tag().name` directly in a script is not checked.)
Names are compared exactly: letter case counts, and so does the alphabet - a
Cyrillic `с` is not a Latin `c`, though they look the same.

So a **path** of names leads to one entity:

| Path | From | Leads to |
|---|---|---|
| `"props/chair"` | an entity | its child `props`, and that one's child `chair` |
| `"../door"` | an entity | its parent's child `door` - a sibling |
| `"/player/camera"` | anywhere | from the level's root; `"/"` is the root |
| `"~/camera"` | an entity | from the root of the prefab instance it is in; `"~"` is that root |
| `"."` | an entity | the entity itself |

`~` is the nearest of the entity and its ancestors that is an instance's root
(it has a `PrefabInstance`) - for a prefab inside a prefab, the inner one.
Outside any instance it is the scene's root, which in the Prefab Editor is the
prefab itself, so `~/...` means the same while editing the prefab and in every
instance of it. Script: `entity:get_prefab_root()`.

In scripts:

| | |
|---|---|
| `entity:find( path )`, `level:find( path )` | The `Entity`. When nothing is there, an error that says which part is missing where, what is there instead, and a name that looks the same but is spelled differently. |
| `entity:try_find( path )`, `level:try_find( path )` | The `Entity`, or `nil`. |
| `entity:get_path()` | `"/player/camera"`. |
| `entity.name` | Reads and sets the name. |

```
find( "camera" ): '/player/playing sphere' has no child 'camera'. Its children:
'mesh', 'ligh', 'сamera'. 'сamera' looks like it but is spelled differently -
its letters 'с' (U+0441) are not Latin.
```

In editor scripts `editor.find( path )` (errors) and `editor.try_find( path )`
(`nil`) give an id, and operators take a path (from the root) wherever they
take an entity id. Functions: `FindByPath`, `WhyPathFails`, `PrefabRootOf`,
`PathOf`, `RelativePath`, `NameOf`, `UniqueChildName`, `MakeNameUnique` in
`engine/scene/hierarchy.hpp`.

### NodePath: references by path

A **NodePath** is a value for State tables that names an entity by the way
to it from the entity that owns the table: `"../door"`, `"part"`,
`"/player"`. In the inspector add a field of type `NodePath` and pick the
target from the tree; it shows `(nothing there)` when the path leads nowhere.
Files keep it as `{ "__type": "NodePath", "path": "../door" }`.

When the game starts, and for whatever `spawn` and `spawn_prefab` make, every
NodePath in a State table (nested tables too) is **replaced by the entity it
leads to**, before any `on_start` - scripts get entities. One that leads
nowhere becomes `nil`, with a warning in the log. A script can make one too:
`NodePath( "../door" )`, and turn it into an entity with `entity:find( p.path )`.
Inside a prefab, `"~/..."` names a part from the prefab's root wherever the
owner sits in it.

NodePath is the kind of reference the inspector puts in State. Unlike an
entity id, a path holds wherever the entities are copied: a prefab's NodePath
to one of its own parts leads to that instance's part in every instance, and
still does after the instance is updated from the prefab (its inner entities
get new ids). An entity a script puts in State at run time stays an entity.

Renaming or moving the target does not update NodePaths that lead to it.

**Physics.** Bodies and character controllers live in the world: they start
at the entity's world transform, and what they move to is written back as
the local transform against the parent. A dynamic body under a moving parent
is not dragged along by it - it is simulated; it is the other way round,
children of a body follow the body.

**Scripts.** `entity.position` / `rotation` / `scale` are local. Also:

| | |
|---|---|
| `entity.world_position`, `entity.world_rotation` | Read only, as of the last world update. |
| `entity:get_parent()` | `Entity`; the root's parent is `nil`. |
| `entity:set_parent( other, keep_world )` | `nil` puts it under the level's root. `keep_world` (default `true`) keeps it where it is; `false` keeps its local transform. `false` on a loop or for the root. |
| `entity:get_children()` | Array of `Entity`. |
| `entity:find( path )`, `entity:try_find( path )`, `level:find( path )`, `entity:get_path()`, `entity:get_prefab_root()`, `entity.name` | See *Names and paths* above. |

A script that places a child by the world position of something else - the
old follow-the-capsule pattern - should either make it a child and drop that
code, or set the local position relative to the parent.

## Prefabs

A prefab is a group of entities kept in a file of its own
(`something.prefab`) and placed into levels as often as wanted. The file is
the same format as a level and is edited the same way, in its own window. Its
root is the prefab: on load it is named after the file.

### Making one

- *Entities* → right click an entity → **Save as prefab** → a path relative
  to the project (`prefabs/crate`; `.prefab` is added). The entity becomes the
  prefab's root, moved to its origin, and everything under it comes along.
  The level is not changed. A folder works as well as any entity.
- Or in the **Prefab Editor**, type a path next to **New**.

### Placing one

- Drag a `.prefab` from the *Project* window onto an entity in *Entities* (it
  goes under that entity) or onto a viewport (in front of the camera).
- Operator `prefab.instantiate{ file = ..., parent = entity id, spawn_at = {x, y, z} }`.
- At run time: `spawn_prefab( "prefabs/crate.prefab", vec3( 0, 5, 0 ) )` →
  the root `Entity`, under the level's root. Bodies join the physics world, sounds set to play on
  start play, scripts get `on_start` - as for a level being loaded.

An instance is a copy of the prefab's root and everything under it. The root
carries a `PrefabInstanceComponent` naming the
file, and shows `[prefab]` in the tree. References between the prefab's
entities in their State tables point at the copies.

### Editing one

**Windows → Prefab Editor**, or double click a `.prefab` in *Project*, or
right click an instance → **Open ...**. The window has its own tree,
inspector, viewport, camera, undo history, selection and clipboard: Ctrl+Z,
Delete, copy and paste act on the prefab while the window has the focus, on
the level otherwise. The prefab is lit by a light of the preview's own on top
of its lights. It can be edited while the game runs.

**Ctrl+S** (or **Save**) writes the file and updates the prefab's instances
in the open level, as one undo step there.

### Updating instances

An update makes each instance again from the file:

- the root keeps its entity id - what names it keeps working - its position
  and its rotation; its scale and everything else come from the prefab;
- everything under the root is replaced - hand edits there are lost, and
  entities under it get new ids.

Updates happen on save from the Prefab Editor (for the open level), from
**Level → Update prefab instances** (every prefab the level uses), from an
instance's **Update from prefab**, or with
`prefab.update_instances{ file = ... }`. Other levels are updated when they
are open and one of those is run.

**Unpack** an instance by removing its `PrefabInstance` component: plain
entities from then on.

### Not done

- Per instance overrides: an update replaces everything under the root.
- Prefabs inside prefabs are copies too: saving prefab B does not update the
  B instances baked into prefab A's file - open A and update its instances
  (**Level → Update prefab instances** works in the Prefab Editor too).
- A prefab cannot contain itself directly; a longer loop (A in B in A) is not
  caught.
- NodePaths are not updated when what they lead to is renamed or moved.
