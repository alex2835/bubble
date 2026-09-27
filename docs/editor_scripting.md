# Bubble — editor scripting

The editor is driven by **operators**: named verbs with JSON arguments
(`scene.create_node`, `entity.add_component`). Every menu item and hotkey
invokes one, and so can a Lua script — in the **Console** window, or with
`bubble_editor -p project.bubble -s script.lua`. The editor's Lua state is its
own; gameplay scripts (`docs/scripting.md`) never see it.

```lua
editor.ops.scene.create_node{ type = "Light", spawn_at = vec3( 0, 5, 0 ) }
editor.ops.entity.add_component{ component = "AudioSource" }   -- on the selection
print( editor.undo_name() )                                     -- "Add AudioSource"
editor.undo()
```

## `editor`

| | |
|---|---|
| `editor.ops.<group>.<verb>( args )` | Invoke an operator. `args` is a table of named fields, or nothing. Returns `false` when the operator's poll says it cannot run now; raises with the operator's message when it fails. |
| `editor.invoke( name, args )`, `editor.poll( name, args )` | The same by string name. |
| `editor.enqueue( name, args )` | Run after this frame. For anything that replaces what the windows are showing: `level.open`, `project.open`. |
| `editor.operators()` | Every registered name. |
| `editor.undo()`, `editor.redo()`, `editor.undo_name()` | |
| `editor.selection()` | Selected entity ids. |
| `editor.select( id, ... )`, `editor.deselect()` | |
| `editor.tree()` | `{ entity, name, folder, children = {…} }` from the level's root. Entity ids are what `parent` arguments take. |
| `editor.entities_by_tag( name )` | Entity ids. |
| `editor.current_level()`, `editor.levels()` | Relative paths, as `level.open` takes. |
| `print( ... )` | To the console. Tables are opened up on one line: `{ 1, { a = 2 } }`. |
| `dump( value, depth )` | A value laid out over several lines, tables to `depth` levels (default 4), keys sorted, a cycle shown as `<cycle>`. |

The **Console** window is the engine's log - everything logged, the game's
output while it runs, and what scripts print - coloured by level (Info,
Warnings, Errors, Scripts), with a filter, Copy and Clear. It stays at the end
until you scroll up. The same log goes to `bubble_editor.log` next to the
editor's executable: the editor has no terminal window (configure with
`-DBUBBLE_EDITOR_CONSOLE=ON` to get one back). In game scripts `print` and
`dump` write to this log too.

At the console a line that is an expression - `editor.tree()`, `editor.selection()`,
`1 + 2` - shows its value, as the standalone Lua prompt does; anything else runs
as a statement. ↑ / ↓ recall what was entered.

Arguments cross as JSON: numbers, strings, booleans, nested tables (an array
when its keys are `1..n`), and `vec2` / `vec3` / `vec4` (as `[x, y, z]`).

## Operators

Where an argument is optional, the default comes from the selection.

| Operator | Arguments | |
|---|---|---|
| `scene.create_node` | `type` (`Folder`, `ModelObject`, `PhysicsObject`, `GameObject`, `Script`, `Light`, `Camera`, `Audio`), `parent?` entity id, `spawn_at?` vec3 | Creates and selects. Without `parent`: into the selected folder, next to another selected entity, else under the root. |
| `scene.delete` | | The selection. |
| `scene.cut`, `scene.copy` | | The selected entity and what is under it. Not the root. |
| `scene.paste` | `parent?` entity id | Cut moves, copy duplicates. Either way the pasted entities stay where they were in the world. |
| `scene.move` | `entity`, `parent` (entity id), `index?` | Reparents, or reorders among the same siblings; stays where it is in the world. |
| `entity.add_component`, `entity.remove_component` | `component` name, `entity?` id | Tag cannot be removed. |
| `history.undo`, `history.redo` | | |
| `project.open` | `path` | Enqueue it. |
| `project.save` | | |
| `level.open` | `file` (relative) | Saves the open level first. Enqueue it. |
| `level.new` | `name` | Enqueue it. |
| `level.set_startup` | `file?` | Default: the open level. |
| `game.run`, `game.stop` | | F5 / F6. |
| `window.show` | `window` (`entities`, `viewport`, `project`, `console`, `animation_graph`, `prefab_editor`), `show?` bool | Opens (or closes) an editor window, as its checkbox in the Windows menu does. |
| `prefab.save` | `file` (relative; `.prefab` added), `entity?` id | Writes the entity and what is under it as a prefab, the entity its root. No undo step: the level is not changed. |
| `prefab.instantiate` | `file`, `parent?` entity id, `spawn_at?` vec3 | Places an instance and selects its root. |
| `prefab.update_instances` | `file?` | Makes the instances again from their files, as one step. Default: every prefab used. |
| `prefab.edit`, `prefab.new` | `file` | Open a prefab in the Prefab Editor; make an empty one and open it. Enqueue them. |

Scripts run against the level. The Prefab Editor has its own history,
selection and clipboard; the hotkeys act on it while it has the focus. See
[hierarchy_and_prefabs.md](hierarchy_and_prefabs.md).

Every edit an operator makes is one step in the undo history; an operator
that makes none (`scene.copy`, `game.run`) leaves no step.

## Adding an operator

Register one with `OperatorRegistry::Instance().Register( { name, label,
poll, exec } )` — engine ones in `projects/engine/src/editing/builtin_operators.cpp`,
editor ones (anything touching editor state) in `BubbleEditor::RegisterEditorOperators`.
`exec` changes the document only through commands on `ctx.mHistory`, so
the step is undoable and a script sees the same result a click does.
