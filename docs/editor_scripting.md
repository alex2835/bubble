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
| `editor.select( id, ... )`, `editor.select_node( node_id )`, `editor.deselect()` | |
| `editor.tree()` | `{ id, type, name | entity, children = {…} }` from the root. Node ids are what `parent` arguments take. |
| `editor.entities_by_tag( name )` | Entity ids. |
| `editor.current_level()`, `editor.levels()` | Relative paths, as `level.open` takes. |
| `print( ... )` | To the console. |

Arguments cross as JSON: numbers, strings, booleans, nested tables (an array
when its keys are `1..n`), and `vec2` / `vec3` / `vec4` (as `[x, y, z]`).

## Operators

Where an argument is optional, the default comes from the selection.

| Operator | Arguments | |
|---|---|---|
| `scene.create_node` | `type` (`Folder`, `ModelObject`, `PhysicsObject`, `GameObject`, `Script`, `Light`, `Camera`), `parent?` node id, `spawn_at?` vec3 | Creates and selects. |
| `scene.delete` | | The selection. |
| `scene.cut`, `scene.copy` | | The selected tree node. |
| `scene.paste` | `parent?` node id | Cut moves, copy duplicates. |
| `entity.add_component`, `entity.remove_component` | `component` name, `entity?` id | Tag cannot be removed. |
| `history.undo`, `history.redo` | | |
| `project.open` | `path` | Enqueue it. |
| `project.save` | | |
| `level.open` | `file` (relative) | Saves the open level first. Enqueue it. |
| `level.new` | `name` | Enqueue it. |
| `level.set_startup` | `file?` | Default: the open level. |
| `game.run`, `game.stop` | | F5 / F6. |

Every edit an operator makes is one step in the undo history; an operator
that makes none (`scene.copy`, `game.run`) leaves no step.

## Adding an operator

Register one with `OperatorRegistry::Instance().Register( { name, label,
poll, exec } )` — engine ones in `projects/engine/src/editing/builtin_operators.cpp`,
editor ones (anything touching editor state) in `BubbleEditor::RegisterEditorOperators`.
`exec` changes the document only through commands on `ctx.mHistory`, so
the step is undoable and a script sees the same result a click does.
