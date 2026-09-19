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
`entity:set_animation_controller( path )` or in the inspector. Saving the file
reloads it in the running editor: every animator on it finds its state again
by name (a state that is gone falls back to the entry) and keeps its
parameters; a file that does not parse is reported and the previous version
stays until it is fixed.

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

### Layers

```json
"layers": [
  { "name": "upper", "mask": [ "mixamorig:Spine1", "!mixamorig:Neck" ], "weight": 1,
    "entry": "none",
    "states": { "none": {}, "wave": { "clip": "wave", "loop": false } },
    "transitions": [
      { "from": "none", "to": "wave", "when": "wave",    "duration": 0.1 },
      { "from": "wave", "to": "none", "exit_time": 0.95, "duration": 0.3 }
    ] }
]
```

A layer is a state machine of its own, played over the base on part of the
skeleton: an aim or a wave on the upper body while the legs keep walking. It
shares the parameters with the base and has the same `states`, `entry` and
`transitions`.

- `mask` — joint names, each taken with everything below it; a leading `!`
  takes a subtree back out. Empty means the whole skeleton.
- `weight` — how much of the layer shows within the mask, a number or a
  parameter name. At 1 the layer replaces the base there; at 0.5, half.
- A state with neither `clip` nor `blend` (`{}`) plays nothing: the layer's
  weight fades to zero over the transition's duration, the clip it was
  showing staying up for the fade. On the base, an empty state is the rest
  pose.

- `"additive": true` — the layer adds its clips on top of the pose instead
  of replacing it, each clip as the change from its own first frame. Author
  additive clips from a neutral first frame: a lean, a breathing cycle, a hit
  reaction. Combined with a mask that leaves the root out, it lands on
  whatever the character is doing. The additive version of a clip is built
  the first time a layer asks for it.

Each layer eases its own transitions, independently of the base.

### Root motion

```json
"root_joint": "mixamorig:Hips",
"states": { "run": { "clip": "run", "root_motion": true } }
```

A state with `root_motion` plays its clips with the root joint's horizontal
travel and yaw taken out: the character animates on the spot, and what it
would have moved is reported each frame by `animator:root_delta()` (a vec3 in
the model's own space, unscaled) and `animator:root_yaw_delta()` (radians).
The script applies it - to the transform, or as a velocity to the character
controller - so the animation never moves a body the physics owns. Height and
the other rotations stay in the animation. In a blend, each clip's travel is
weighted by its share. Without a controller, `animator:root_motion( joint )`
does the same for the base playback.

### Events

```json
"events": { "walk": [ [0.32, "footstep"], [0.82, "footstep"] ] }
```

Per clip, `[normalized time, name]` pairs. A script reads what fired with
`animator:events()`; `add_event` adds markers from a script. Markers fire when
the playback crosses them, forwards, backwards, or around the loop; in a blend
the heaviest clip's markers are the ones that count. A marker at `0` fires as
the loop comes round, not on the first frame.

### IK

Applied last, on the composed pose, from a script each frame it wants it,
with world space targets:

- `animator:look_at( joint, target, { weight, forward, up } )` turns the
  joint so its local `forward` (default +Z) points at the target, `up`
  (default +Y) kept as upright as it can. A head.
- `animator:reach( end_joint, target, { weight, pole, mid_axis, soften } )`
  bends the two bones above `end_joint` - foot, knee, hip - to put it on the
  target. `pole` (world) is where the knee points, by default where it points
  now; `mid_axis` is the knee's hinge in its own space, by default read off
  the current bend; `soften` (0.97) keeps the limb from locking straight as
  the target goes out of reach.

Both are ozz's `IKAimJob` and `IKTwoBoneJob`; the joints below the corrected
ones follow.

### What the inspector shows

The current state, every parameter (editable), and each transition out of the
current state with its conditions - green while they hold. That is the whole
debugging story: if a character will not leave a state, the reason is on
screen.

## Not done

- A node editor. The JSON with the live inspector is the source of truth; an
  editor would be a view over it.
- GPU skinning. Skinning is on the CPU per entity (`renderer.md`).
