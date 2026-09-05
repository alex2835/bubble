# Bubble — WebGPU renderer

A map of the renderer, for finding your way around it and for knowing where a
change belongs. It assumes you knew the OpenGL one, and says what moved.

The single idea to carry through everything below: **OpenGL was a state machine
you mutated, WebGPU is a set of objects you build.** Nearly every difference in
this renderer follows from that. `glEnable`, `glBlendFunc`, `glDepthFunc`,
`glBindVertexArray`, `glUniform*` — none of them have an equivalent. What they
used to do is now baked into an immutable pipeline or written into a buffer that
a bind group points at.

---

## Where things live

| File | What it owns |
|---|---|
| `renderer/gpu_context.{hpp,cpp}` | instance, adapter, device, queue, surface. The `Gpu()` accessor. |
| `renderer/pipeline.{hpp,cpp}` | bind group layouts, the pipeline layout, pipeline creation, the uniform rings |
| `renderer/renderer.{hpp,cpp}` | frame and light uniforms, the draw calls |
| `renderer/shader.{hpp,cpp}` | a compiled WGSL module and its cache of pipeline variants |
| `renderer/buffer.{hpp,cpp}` | vertex/index buffers, vertex layout, uniform buffers |
| `renderer/texture.{hpp,cpp}` | texture, view and sampler |
| `renderer/framebuffer.{hpp,cpp}` | a set of render targets and the pass that draws into them |
| `renderer/material.{hpp,cpp}` | the material bind group |
| `renderer/entity_id_picker.{hpp,cpp}` | asynchronous readback for click selection |
| `loader/shader_loader.cpp` | WGSL compilation, error reporting, uniform reflection |
| `loader/shader_module_loader.cpp` | the `#include` expander. Unchanged from the GLSL days. |
| `deps/wgsl_reflect/` | standalone WGSL declaration parser and layout calculator |
| `resources/shaders/*.wgsl` | one file per shader, both entry points in it |
| `resources/shaders/modules/*.wgsl` | includable snippets: `common`, `material`, `light`, `phong` |

There is no `renderer/gl/` equivalent. The cutover was complete; `grep -rn
"gl[A-Z]" projects/` finds only a comment.

---

## The object graph

```
GpuContext              created by Window, destroyed by Window
 ├─ Instance            the API entry point
 ├─ Surface             the window's presentation surface
 ├─ Adapter             a physical GPU + backend (Vulkan here, via wgpu-native)
 ├─ Device              creates every resource; owns validation
 ├─ Queue               submit(), writeBuffer(), writeTexture()
 ├─ StandardLayouts     the four bind group layouts + the pipeline layout
 └─ WhiteTexture        1x1, stands in for absent material maps
```

Reached through `Gpu()`, a process-wide accessor. That is deliberate: OpenGL had
exactly one current context and every renderer class assumed it, so threading a
device through the constructor of every `Texture2D`, `VertexBuffer`, `Shader`
and `Framebuffer` — and through both loaders and the editor — would have been a
very large diff to encode something that is still a single global.

It is valid only between `Window`'s constructor and destructor. `BubbleEditor`
declares `mWindow` before `mEngine` and `mProject`, so every GPU resource is
already gone by the time the context is torn down. Keep it that way.

---

## A frame

```
BubbleEditor::Run()
 │
 ├─ mWindow.PollEvents()
 ├─ mEngine.mRenderer.BeginFrame()        resets the uniform rings
 │
 ├─ Engine::DrawScene( mSceneViewport )   ─┐
 ├─ Engine::DrawEditorBillboards()         │ each records and submits
 ├─ Engine::DrawEntityIds()   (on demand)  │ its own command buffer
 ├─ Engine::DrawCameraFrustums()           │
 ├─ Engine::DrawBoundingBoxes()           ─┘
 │
 ├─ mWindow.ImGuiBegin()                  sizes the surface, starts the UI frame
 ├─ EditorUserInterface::OnDraw()         the viewport is an ImGui::Image
 ├─ mWindow.ImGuiEnd()                    acquires the surface texture, UI pass
 └─ mWindow.OnUpdate()                    present, then poll the device
```

Each `Draw*` opens its own render pass. `DrawScene` clears; everything after it
loads, which is what keeps the overlays from wiping the scene underneath. That
choice lives in one place: `Framebuffer::BeginRenderPass( encoder, clearColor,
clearDepth )` — pass a colour to clear, `std::nullopt` to load.

The scene never touches the window surface. It renders into offscreen
framebuffers that the editor displays with `ImGui::Image`. Only the UI is drawn
to the surface.

---

## The four bind groups

The central design. Grouped by **how often the data changes**, which is the axis
that matters for cost.

| Group | Contents | Set |
|---|---|---|
| 0 `Frame` | camera (proj/view), lights info, lights array | once per pass |
| 1 `Material` | material params + 3 maps + sampler | per mesh |
| 2 `Draw` | model matrix, normal matrix, object id, billboard params | per draw, dynamic offset |
| 3 `User` | the shader's own `UserUniforms` block | per draw, dynamic offset |

Indices match `@group(N)` in the WGSL. Defined in `pipeline.hpp`
(`BindGroupIndex`).

**Groups 2 and 3 use one ring buffer each**, not a bind group per object. Each
draw writes its block into the next slot and the group is set with that slot's
dynamic offset (`DynamicUniformRing`). WebGPU has no push constants — wgpu-native
has an extension but the web backend does not, so it is off limits — and a bind
group per entity would need rebuilding whenever a value changed.

All four layouts are **shared and fixed**, which is what lets one frame bind
group serve every pipeline. A shader that does not use a group simply ignores
it; a layout may declare more than the shader reads. But every group the layout
names must have *something* bound before a draw, which is why the material group
is bound even for shaders that never sample it.

---

## Pipelines

A `RenderPipeline` freezes shaders **plus** everything OpenGL kept mutable:
vertex layout, topology, cull mode, front face, depth test and write, blend,
target formats. You do not change blending; you switch pipeline.

So one shader needs several. `Shader` keeps a small cache keyed on `PipelineKey`:

```cpp
struct PipelineKey {
    wgpu::TextureFormat mColorFormat;   // RGBA8 viewport vs R32Uint id buffer
    wgpu::TextureFormat mDepthFormat;
    DrawingPrimitive    mPrimitive;     // triangles vs the line helpers
    bool                mCullBackFaces;
    bool                mBlend;
    u32                 mAttributeMask; // which vertex attributes the mesh has
};
```

`mAttributeMask` is in the key because the pipeline *declares* the vertex
attributes, so a mesh without tangents genuinely needs a different pipeline from
one with them.

Blending is off. Billboards cut out with `discard`, exactly as they did under
OpenGL — do not "fix" that by enabling blending, it changes the picture.

---

## Vertex data

Blocked, not interleaved: every position, then every normal, then texcoords,
tangents, bitangents. WebGPU handles that by binding the **same buffer to
several vertex slots at different offsets**, one slot per attribute
(`VertexArray::Bind`).

Locations are fixed by semantic, and match the `@location(N)` in the WGSL:

| 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| Position | Normal | TexCoords | Tangent | Bitangent |

Two rules, and both were bugs before:

- **An attribute with no data produces no slot.** `VertexLayout::FromData` skips
  empty arrays. The old layout emitted all five with the count taken from a
  possibly-empty array, so a mesh without tangents got attributes pointing past
  the end of its own buffer. OpenGL tolerated it; WebGPU rejects it at pipeline
  creation.
- **A shader's inputs must all be provided.** The converse. The lit shaders
  declare all five, so `ProcessMesh` sizes all five to the vertex count and
  leaves the missing ones zeroed. A mesh with no tangent basis cannot be normal
  mapped anyway.

---

## Shaders

One `.wgsl` per shader, holding **both** entry points — `vs_main` and
`fs_main`. WGSL has no separate vertex and fragment files, so the old "a `.frag`
with no `.vert` inherits `phong.vert`" fallback is gone. `PhongVertex()` in
`<phong>` is that default vertex stage; call it explicitly.

The `#include <module>` expander is unchanged. It still has **no include
guards**: `<phong>` pulls in `<material>` then `<light>`, and including
`<material>` yourself as well is a duplicate-declaration error. The module name
also sets a feature bit (`ShaderModule::Material`, `ShaderModule::Light`) that
the engine reads.

Errors come back through `pushErrorScope` and print the **expanded** source with
line numbers. The expander emits no `#line` directives, so a reported line number
means nothing without that listing.

### A shader's own uniforms

WebGPU exposes no reflection — not through `webgpu.h`, not in wgpu-native, not
in the browser — so `deps/wgsl_reflect` reads the declarations out of the source.
Hot reload needs this at runtime for arbitrary user shaders, which is why it
cannot be a build step.

Declare them in a struct named exactly `UserUniforms`:

```wgsl
struct UserUniforms
{
    uColor: vec4<f32>,     // @default(1, 1, 1, 1)
    uStrength: f32,        // @default(0.5)
};
@group(3) @binding(0) var<uniform> uUser: UserUniforms;
```

The engine reflects that struct to build the inspector, drive the Lua table, and
find the byte offset each value is written at.

**The default lives in a comment** because WGSL, unlike GLSL, does not allow an
initializer on a uniform. There is nowhere else to put it. It is the value a
fresh `ShaderComponent` starts at, and it seeds the block before the Lua table is
applied — a uniform the table has no entry for lands on its default, not on zero.

Limit is 256 bytes (`cUserUniformBlockSize`); a larger block is rejected at load.
Textures declared at group 3 are **not** wired yet — only the uniform buffer is.

---

## Picking

`glReadPixels` blocked until the GPU drained. WebGPU has no synchronous readback
at all, so `EntityIdPicker` copies the id texture into a buffer, maps it, and the
result arrives a frame or two later.

```
frame N    click → EntityIdPicker::Request( rect )
frame N+1  WantsIdPass() is true → DrawEntityIds → CaptureFrom()
frame N+2  TakeResult() → selection
```

Less of a change than it sounds: the selection already did not take effect until
the next frame, because the gizmo reads `mSelection` *before* the click that set
it is processed.

The id pass now only runs on frames where something asked to pick. It used to run
every frame — a second full traversal of the scene plus a billboard per camera
and light, whether or not anyone had clicked.

`Cancel()` on viewport resize and on leaving editing mode, or a readback in
flight is reading an attachment that no longer exists.

---

## How to do things

**Add a shader.** One `.wgsl` in `resources/shaders/` (or the project's shader
directory). `#include <common>` for the frame and draw bindings, `#include
<phong>` if you want lighting. Give it `vs_main` and `fs_main`.

**Add a per-entity uniform.** Add a field to that shader's `UserUniforms` struct
with a `// @default(...)` comment. Nothing else — the reflection picks it up, the
inspector shows it, and Lua can set it by name.

**Add an engine-wide uniform.** Add it to the relevant block in
`Renderer::Renderer()` *and* to the matching WGSL struct in
`modules/common.wgsl` or `modules/light.wgsl`. The two layouts must agree byte
for byte; `deps/wgsl_reflect`'s tests pin the ones that already exist.

**Add a render pass.** Follow `Engine::DrawBoundingBoxes`: `SubmitPass` with a
lambda that opens a `Framebuffer::BeginRenderPass`, binds the frame group, and
draws. Pass `std::nullopt` as the clear colour unless you mean to wipe the target.

**Debug a validation error.** Read the log — wgpu-native's messages are good and
name the object label. Every pipeline, pass, buffer and bind group here is
labelled for that reason.

---

## Traps

Every one of these cost real debugging time. They are not obvious from the docs.

**`wgpu::Default` is wrong for bind group layout entries.**
`BindGroupLayoutEntry::setDefault()` sets the sub-layouts it is not using to
`Undefined` — but `Undefined` is **1** and `BindingNotUsed` is **0**, and it
leaves `texture.viewDimension` at `2D`. wgpu then resolves a buffer binding as a
texture one. **Zero-initialize layout entries** (`= {}`) and set only the one
sub-layout you mean.

**A render pass must be *ended*, not just released.** Releasing the encoder
leaves the command encoder locked and the next thing recorded on it fails.
`RenderPassScope` ties `end()` to scope exit so it cannot be forgotten.

**The surface texture must stay alive for the whole frame.** The view alone does
not keep it from being destroyed, and the commands referencing it are not
submitted until later. `GpuContext` holds it until `Present()`.

**`depthSlice` must be `WGPU_DEPTH_SLICE_UNDEFINED`** on a colour attachment for
a 2D target. Leaving it zero is a validation error.

**Size the surface from a live query, not the resize callback.**
`ImGui_ImplGlfw_NewFrame` asks the OS for the window size directly, while a GLFW
resize callback is only delivered when the message queue is pumped. On the frame
a window changes size those disagree, and the UI pass sets a viewport its render
target cannot contain.

**WebGPU's texture origin is the top left**, OpenGL's was the bottom left. That
governs the `ImGui::Image` UVs and the picking coordinates — both measure down
from the top now. It does *not* govern the billboard shader's texture coordinate
flip, which compensates for the quad's own UV assignment and is API-independent.

**There is no three channel texture format.** The loader expands 24 bit images to
RGBA on the way in.

**`bool` is not host-shareable.** A shader spells a boolean uniform `u32`. The
inspector still edits it as a checkbox.

---

## What is not done

- **User textures in group 3.** Only the uniform buffer is wired. A shader that
  declares `@group(3) @binding(1) var myMap: texture_2d<f32>` will not get it.
- **The Emscripten build is untested.** The distribution makes the web target a
  backend flag rather than a port, but nothing has exercised it. Refuse any
  wgpu-native-only feature — push constants especially — until it has been.
- **`Cubemap` and `Skybox` were deleted, not ported.** They were loaded but had
  no draw path at all.
- **No golden-image tests.** The engine has no test suite; `deps/wgsl_reflect` is
  the only tested piece. A capture-and-diff harness is the cheapest thing that
  would catch a depth-range or texture-origin regression.

---

## Source

`projects/engine/src/renderer/`, `projects/engine/src/loader/shader_loader.cpp`,
`deps/wgsl_reflect/`, `resources/shaders/`.

Vendored versions are recorded in `deps/*/VENDORED_VERSION.txt`. The WebGPU
implementation is chosen by `WEBGPU_BACKEND` (`WGPU`, `DAWN`, `EMSCRIPTEN`,
`EMDAWNWEBGPU`), defaulting to wgpu-native natively and to emdawnwebgpu under
`emcmake`.
