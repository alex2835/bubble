# Bubble — skeletal animation

What a skinned model brings with it, how it is played, and the `.anim`
controller format. The script side is in `docs/scripting.md` (*Animator*); the
renderer side in `docs/renderer.md` (*Skinned meshes*).

## The pieces

| | |
|---|---|
| `engine/animation/skeleton.hpp` | `Skeleton` (ozz skeleton + inverse bind matrices) and `AnimationClip`, imported with the model by `loader/skeleton_loader.cpp` |
| `engine/animation/animator.hpp` | `Animator` — samples, blends, eases, poses and skins one entity |
| `engine/animation/inertialization.hpp` | how transitions ease |
| `engine/animation/blend_space.hpp` | clips along a parameter |
| `engine/animation/animation_controller.hpp` | the `.anim` asset and its evaluation |
| `scene/components/animator_component.hpp` | the playback state on an entity, and its per-frame `Advance` |

Runtime is [ozz-animation](https://github.com/guillaumeblanc/ozz-animation)
(`deps/ozz`); import is assimp, so glTF and FBX both work and nothing is
converted offline. Clips are addressed by name - the animation's name in the
file, or `clip_0`, `clip_1`, … for unnamed ones.

A model with bones gets a `Skeleton` and its clips on `Model`; an entity with
that model and an `AnimatorComponent` is posed every frame by
`Engine::UpdateAnimations`, after the scripts and in the editor too.

## Playing

Three levels, each built on the one below:

1. **A clip.** `play( name, seconds )`. Loops, or stops at the end.
2. **A blend space.** `play_blend( name, points )` + the `blend` parameter.
   Every clip in the space runs on one normalized phase so gaits stay in step.
3. **A controller.** A `.anim` file decides between 1 and 2 from parameters
   the script sets. This is the one to use for a character.

Every switch is **inertialized** rather than cross faded: the pose's offset
from the new clip, and its velocity, are recorded at the switch and decay to
zero over the transition time. The old clip is not evaluated after the switch,
and a transition may interrupt a transition without a pop.

## The `.anim` controller

JSON, next to the model, listed under `controllers` in the project file and
picked up by the Project window. Attached with
`entity:set_animation_controller( path )` or in the inspector.

```json
{
  "parameters": { "speed": 0, "grounded": true, "attack": "trigger" },
  "entry": "locomotion",
  "states": {
    "locomotion": { "blend": { "param": "speed",
                               "points": [ ["idle", 0], ["walk", 1.5], ["run", 4] ] } },
    "jump":       { "clip": "jump", "loop": false },
    "attack":     { "clip": "attack_1", "loop": false, "speed": 1.2 }
  },
  "transitions": [
    { "from": "*",          "to": "jump",       "when": "!grounded",   "duration": 0.1, "interrupt": true },
    { "from": "jump",       "to": "locomotion", "when": "grounded",    "duration": 0.2 },
    { "from": "locomotion", "to": "attack",     "when": "attack",      "duration": 0.05 },
    { "from": "attack",     "to": "return",     "exit_time": 0.9,      "duration": 0.2 }
  ]
}
```

### Parameters

`name: default`. A number is a float, `true`/`false` a bool, `"trigger"` a
trigger. A script writes them with `set` and `trigger`; the inspector shows
and edits them live.

A **trigger** is set by the script and cleared by the controller: the
transition that fires on it consumes it, and one nothing fired on is dropped
at the end of the same frame. A queued attack never goes off two seconds late.

### States

Exactly one of:

- `"clip": name` — with `"loop"` (default true) and `"speed"` (default 1; a
  number, or a parameter name to read every frame).
- `"blend": { "param": name, "points": [ [clip, value], … ] }` — a blend space
  on that parameter. Always loops.

`"entry"` names the state a fresh controller starts in; default is the first.

### Transitions

Checked every frame, **any-state (`"from": "*"`) ones first, then the current
state's, each in file order; the first that holds fires.**

| Field | |
|---|---|
| `from` | a state, or `"*"` for any. `"*"` never fires into the state already playing. |
| `to` | a state, or `"return"`: the state the current one was entered from. One edge out of a one-shot, instead of one per state it could have interrupted. |
| `when` | a condition, or an array of them, all of which must hold |
| `exit_time` | normalized time 0..1 the state must have reached, or passed this frame while looping. `0` fires on the state's first frame. |
| `duration` | seconds to ease into the new state (default 0.2) |
| `interrupt` | may fire while the pose is still easing into the current state (default false, so a chain of transitions cannot flicker through three states in one frame) |

A condition is `"name"` (true / trigger set), `"!name"`, or
`"name <op> value"` with `==`, `!=`, `<`, `<=`, `>`, `>=` and a number or
`true`/`false`. A transition needs a `when`, an `exit_time`, or both.

Errors in the file are reported with the file name and what is wrong, and the
controller does not load.

### Events

```json
"events": { "walk": [ [0.32, "footstep"], [0.82, "footstep"] ] }
```

Per clip, `[normalized time, name]` pairs. A script reads what fired with
`animator:events()`; `add_event` adds markers from a script. Markers fire when
the playback crosses them, forwards, backwards, or around the loop; in a blend
the heaviest clip's markers are the ones that count. A marker at `0` fires as
the loop comes round, not on the first frame.

### What the inspector shows

The current state, every parameter (editable), and each transition out of the
current state with its conditions - green while they hold. That is the whole
debugging story: if a character will not leave a state, the reason is on
screen.

## Not done

- Layers with joint masks (upper body over locomotion), additive clips, IK
  and root motion. The `Animator` blends any number of layers already; the
  masks and the asset syntax for them are the missing part.
- A node editor. The JSON with the live inspector is the source of truth; an
  editor would be a view over it.
- GPU skinning. Skinning is on the CPU per entity (`renderer.md`).
