# Bubble — hierarchy and prefabs

## Hierarchy

An entity can hang under another. Its `TransformComponent` is then relative
to its parent: moving, turning or scaling the parent carries the child with
it.

| Piece | Where | What |
|---|---|---|
| `HierarchyComponent` | `scene/components/hierarchy_component.hpp` | `mParent`, `mChildren`. Only on entities that have either. The parent is saved; the children are rebuilt. |
| world cache | `TransformComponent::World()`, `WorldMatrix()` | Where the entity is in the world. Filled by `UpdateWorldTransforms`. Not saved. |
| functions | `engine/scene/hierarchy.hpp` | `SetParent`, `ParentOf`, `ChildrenOf`, `IsAncestor`, `ComputeWorldMatrix`, `SetWorldTransform`, `UpdateWorldTransforms`, `SyncHierarchy` |

**Local vs world.** The transform's own fields - what the inspector, the
gizmo's result, files and scripts' `position`/`rotation`/`scale` read and
write - are local. Everything that places something in the world reads the
cache: drawing, picking, physics, lights, audio, cameras, IK targets, the
skeleton overlay. The engine refreshes the cache after physics and again
after the scripts; the editor refreshes it every frame.

**In the editor the tree is the truth.** An entity node under another
entity's node is that entity's child - folders in between are see-through, so
an entity in a folder under an entity is still its child. `SyncHierarchy`
writes the tree into the components every frame; a loaded level gets its
components from its tree the same way, so levels saved before the hierarchy
open unchanged.

- **Parent** by dragging a node onto another in *Entities* (or cut and paste
  into it). The dragged entity keeps its place in the world; its local
  transform is worked out against the new parent. Dropping onto the level's
  root makes it a root again. Undo puts the local transform back.
- **Selecting** an entity selects that entity only; what hangs under it
  follows. Selecting a folder selects the entities at its top.
- **The gizmo** moves the entity in the world (Shift for world axes) and
  writes the local transform back.
- New entities made under an entity land where they were asked to in the
  world, as before.

**Physics.** Bodies and character controllers live in the world: they start
at the entity's world transform, and what they move to is written back as
the local transform against the parent. A dynamic body under a moving parent
is not dragged along by it - it is simulated; it is the other way round,
children of a body follow the body.

**Scripts.** `entity.position` / `rotation` / `scale` are local. Also:

| | |
|---|---|
| `entity.world_position`, `entity.world_rotation` | Read only, as of the last world update. |
| `entity:get_parent()` | `Entity` or `nil`. |
| `entity:set_parent( other, keep_world )` | `nil` makes it a root. `keep_world` (default `true`) keeps it where it is; `false` keeps its local transform. `false` on a loop. |
| `entity:get_children()` | Array of `Entity`. |

A script that places a child by the world position of something else - the
old follow-the-capsule pattern - should either make it a child and drop that
code, or set the local position relative to the parent.

## Prefabs

A prefab is a group of entities kept in a file of its own
(`something.prefab`) and placed into levels as often as wanted. The file is
the same format as a level - scene and tree - and is edited the same way, in
its own window.

### Making one

- *Entities* → right click a node → **Save as prefab** → a path relative to
  the project (`prefabs/crate`; `.prefab` is added). The node and everything
  under it are written, moved so that the first entity sits at the prefab's
  origin. The level is not changed.
- Or in the **Prefab Editor**, type a path next to **New**.

### Placing one

- Drag a `.prefab` from the *Project* window onto a node in *Entities* (it
  goes under that node) or onto a viewport (in front of the camera).
- Operator `prefab.instantiate{ file = ..., parent = node id, spawn_at = {x, y, z} }`.
- At run time: `spawn_prefab( "prefabs/crate.prefab", vec3( 0, 5, 0 ) )` →
  the root `Entity`. Bodies join the physics world, sounds set to play on
  start play, scripts get `on_start` - as for a level being loaded.

An instance is a copy, with a root: when the prefab has one entity at its
top, that entity is the root; otherwise an entity named after the prefab is
made to hold them. The root carries a `PrefabInstanceComponent` naming the
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
