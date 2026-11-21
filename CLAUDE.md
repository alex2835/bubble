# Bubble

A small 3D engine rebuilt from the old one (`E:\projects\bubble0.7`, kept
until parity) a stage at a time. Decisions live in the architecture doc,
"Bubble: архитектура движка":
https://claude.ai/code/artifact/cfe42e9b-bcd3-4e0b-bed7-338f80567534 -
read the section you touch before changing a design, and update the doc when
a decision changes. It also holds the stage plan: a feature waits for its
stage, however ready it looks in the old code.

The sandbox - the game stages 7 to 9 are checked against - is a project of
its own, outside the engine: `E:\projects\bubble_sand_box2`, in the new
formats. The old one, `E:\projects\bubble_sand_box`, is read only: its
assets and scripts are copied and rewritten, never converted by code.

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
  `check_cxx_compiler_flag`. Clang 20 cannot deduce the arguments of an
  alias template either: `vector<T>{ ... }`, `pair<A, B>{ ... }`, never
  `vector{ ... }` or `pair{ ... }` - our containers are aliases.
- Run release too when touching anything with lifetimes or the Luau stack.
- Format what you change with VS's clang-format before finishing:
  `"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe" -i <files>`
  (only `projects/`, `deps/luaubind/` and `deps/wgsl_reflect/`, never vendored deps).

## Layout

```
deps/                         vendored, each with its license; listed in THIRD_PARTY_NOTICES.md (ozz built by our
                              own CMake: its runtime in the engine, its offline builders in import; Yoga in the
                              engine; FreeType - TrueType and OpenType modules - and msdfgen in import; Dear ImGui's
                              core - docking, no backends -, ImGuizmo and Roboto, the font built into it, in the
                              editor).
                              wgpu-native alone is downloaded at configure time into out/downloads, pinned by version
                              and SHA-256
deps/luaubind/                our own small sol-like C++ binding for Luau; depends on Luau only
deps/wgsl_reflect/            our own reflection of WGSL source: structs laid out for uniforms, bindings, functions,
                              the comment on a line; the standard library only
projects/engine/include/bubble/
    types/                    aliases (std, glm; matrix.hpp for mat4/quat), Handle/SlotMap, OptRef
    core/                     log, profile, utf8, AssetPath, process, Name, Text, Color, Random (PCG32), Jobs (the
                              process's one pool on enkiTS: ParallelFor, Background, RunOnMain; the web and Jobs( 1 ):
                              the main thread alone)
    assets/                   AssetId (random, 32 hex), AssetCatalog (id <-> path), AssetRegistry (entries by id
                              and type - a source imported as several kinds, a model with its skeleton and clips -,
                              paths through the catalog, loaders by kind reading the import cache - ImportedPath,
                              .bubble/imported/<id>.<kind> - on Jobs' pool (Load: Loading until the main queue
                              runs, then OnLoaded) or here and now (LoadNow); reads hand their bytes over at once
                              (AtOnce, FromDirectory) or later (FromFetch on the web), AssetRef<T>, reload events;
                              Set: an asset made in memory under an id of its own, a load of the id finds it);
                              TextureAsset (RGBA8 mip levels, its bytes "BTEX", RegisterTextureLoader), ModelAsset
                              (meshes: vertices, a skin's joints and weights beside them, LOD levels sharing them,
                              the box; the joints - name, rest, inverse bind -; "BMDL", RegisterModelLoader;
                              SkinPose: a skinned model's matrices this frame, runtime); FontAsset (an MSDF atlas -
                              a TextureAsset of data -, metrics, glyphs, kerning; "BFNT", RegisterFontLoader);
                              MakeBox/MakePlane/MakeSphere (models made in code); ShaderAsset (a material's WGSL),
                              MaterialAsset (.mat JSON: shader or base, double_sided, transparent, params by name);
                              PrefabAsset (a .prefab's text, parsed where it is used)
    animation/                on ozz's runtime (src/animation alone): animation_graph (.anim: parameters, states - a
                              clip, a blend along a parameter, nothing -, transitions by conditions and exit time,
                              triggers, "return", events at shares of a clip, root motion on a state; parsed, and
                              stepped as plain data), animation_assets (the skeleton and clips a model's importer
                              writes beside it under its id; RegisterAnimationLoaders), AnimationModule: animator
                              (graph, speed) - the graph stepped each fixed step in Phase::Physics, its events
                              ScriptCalls to on_animation_event; poses sampled each frame between the last two steps
                              on Jobs' pool, transitions eased by inertialization, written as the model's SkinPose -,
                              bone_attachment (an entity hung on a joint of its parent's pose: ParentOffset);
                              AnimationWorld (a service: set, trigger, state, joints, root motion); scripts'
                              animator:set/trigger, animator.state, root_velocity, root_turn
    input/                    controls (Key by USB HID usage - SDL's scancodes -, mouse and gamepad buttons, axes; their
                              snake_case names), InputEvent (the engine's own: keys, mouse, touches as shares of the
                              window, gamepads), ActionMap (actions by name bound to keys, buttons, axes with a scale,
                              touch areas, the mouse's x, y and wheel - how far it went since the last latch, a
                              camera's turn -), Input (the devices' state -> actions; LatchStep/LatchFrame each see a
                              press once, at any frame rate)
    audio/                    on miniaudio (src/audio alone): AudioDevice (one a process: the mix, buses music, sfx,
                              ui, voice under master, their volumes; Offline for tests, read with ReadFrames;
                              Resume after a browser's gesture), SoundAsset (decoded whole, or streamed from its file
                              - SoundData shared with the voices -), AudioModule: audio_source, audio_listener (or the
                              first camera), 32 voices a world - the quietest gives way -, a paused world silent but
                              for ui; AudioWorld (a service: PlaySound, a source's play/stop, SetAudible); scripts'
                              audio.play_sound/play_sound_at/set_bus_volume and audio_source:play()/stop()
    physics/                  PhysicsModule on Jolt (src/physics alone): box, sphere, capsule and mesh colliders,
                              rigid_body (dynamic, or kinematic: moved by its transform), character_controller
                              (CharacterVirtual: a velocity set each step, ground, steps, slopes, its weight on what
                              it stands on - mass, 10 by default, Jolt holding about 10:1 -); stepped in
                              Phase::Physics on one thread, what moved written into transforms, contacts as
                              ScriptCalls to on_collision; PhysicsWorld (a service: raycast, overlap_sphere,
                              velocities, characters); snapshots keep Jolt's state, bodies made again under their ids
    platform/                 Platform (SDL, seen nowhere else: one a process, Poll -> InputEvents - TextInput among
                              them -, gamepads opened as they come, QuitRequested, Run - a loop, or the browser's
                              frames, SetCursor - the pointer's shape -, the clipboard's text, TryOpen - a file the
                              system's way -), Window (the cursor
                              held for a camera: SetCursorLocked; SetTextInput)
    render/gpu/               Gpu (WebGPU's device: wgpu-native on the desktop, emdawnwebgpu on the web; Start ->
                              GpuStartup::TryFinish each frame, since a browser answers between frames; TryStartNow
                              for tests; Check/TryCheckNow - validation errors caught, WGSL's with their lines;
                              Submit, Clear, TryReadRgba8), gpu_binding (bind group layouts and groups an entry at a
                              time), GpuSurface (the window's swap chain, sRGB views,
                              Resize/TryAcquire/Present), GpuHandle (WGPU objects owned; aliases ...Handle),
                              GpuBuffer, GpuTexture (a texture and its view; from a TextureAsset with its mips),
                              CreateShader/CreateRenderPipeline, GpuModel (a ModelAsset's meshes in one vertex and
                              one index buffer, LODs as index ranges, a skin's joints and weights in a second vertex
                              buffer, its rest pose's matrices), GpuAssets (the GPU's copies of the
                              registry's models and textures by id, made again after a reload, Collect),
                              Renderer (one a process; Draw( view, scene, camera, target ): shadow map, opaque then
                              transparent surfaces in rgba16float with reversed depth, ACES tonemapping; frustum
                              culling, each mesh's LOD by its error in pixels with hysteresis, meshes alike in one
                              instanced call; skinned meshes through their own pipelines - vs_skinned -, each
                              instance's joint matrices in one storage buffer, its pose's or its model's rest; the
                              debug drawing's lines; a UiScene's world canvases among the surfaces and its screen
                              canvases over the tonemapped picture, batched by texture; materials
                              built from their bases, shader and textures in src/render/gpu/gpu_materials - the
                              standard one while they load, a magenta placeholder when they fail), RenderView (one
                              place a world is seen from: its targets, LOD hysteresis, Pick/TryPicked - the entity
                              at a pixel, back when the GPU has drawn that frame -, Stats), EngineShader (the WGSL of
                              projects/engine/shaders, built in by cmake/embed_shaders.cmake); WGPU types stop here
    render/                   render_scene: camera, directional_light, point_light, spot_light, model (asset,
                              materials by mesh, tint); RenderModule gathers them each frame into RenderScene (items,
                              lights, cameras, time, debug lines), a world's service; DebugDraw (a world's service:
                              lines, boxes, spheres, axes for one frame); material_layout: TryReflectMaterial - what a
                              material's WGSL declares, against the contract (group 2, one uniform struct, textures,
                              samplers, fn surface; @default and @color in a line's comment)
    scene/                    Entity, EntityPath, Scene (an EnTT registry; components by type; listeners of a type's
                              components and of fields set), Hierarchy (the tree from the root, names unique among
                              siblings, PathOf/Resolve), ComponentTypes (TrySetField tells the scene),
                              SaveScene/LoadScene (nested JSON with "version" and the level's own "asset" id),
                              FileContext (component types, catalog, prefabs by id), one entity to JSON and back;
                              Prefab (nodes with ids that last, next_id), Instantiate, an instance in a level as
                              its prefab (by id) + overrides (set, add_components, add_children, remove);
                              PrefabInstance and PrefabLink are ComponentTypes' internal types: kept by undo, never
                              in files;
                              SceneSnapshot (the data in memory: entity numbers exact, every type's order kept)
    data/                     Value (what a file keeps for a prop), ValueType, Check; Json (our own small
                              parser and writer), BinaryWriter/Reader (little-endian, for imported assets), Value
                              <-> Json (plain by type, tagged for Any; an asset is { "asset": id, "path": path },
                              found by the path when the id is not); EncodePng (RGBA8 as a PNG, stored blocks: a
                              screenshot, a reference picture)
    reflection/               Reflection (an entt::meta context): TypeBuilder, FieldInfo (range, flags, tooltip, the
                              asset kind a field takes), Get/Set/Call by name,
                              FieldPath ("shadow.bias", "steps[2]"), Value <-> MetaAny, field listeners
    scripts/                  lua.hpp (luaubind names in bubble; Entity, Color, AssetId cross as Luau types),
                              ScriptAsset (bytecode, props read from the AST, requires resolved; TryResolveRequire),
                              ScriptRuntime; ScriptModule (a world's Luau state and runtime: Script component - asset
                              + props overrides as a Value record -, ScriptInstance runtime, callbacks on_start /
                              on_fixed_update / on_update / on_destroy in the frame's order, the require graph loaded
                              first, self kept by snapshots, the time limit, progress and save slots);
                              ScriptApi (what a module adds to scripts beyond reflection: component members, globals,
                              callbacks; ScriptCall - a module calling a script's callback through the world's events);
                              src/scripts/world_api (entity, color, asset, each component a Luau type from reflection,
                              services described as such - debug_draw -, input, spawn( prefab, parent?, position? )
                              - the prefab parsed once a version, held -, math.random on the world's Random; prefabs a
                              script's props name load as it starts),
                              script_values (Value <-> Luau), script_declarations (bubble.d.luau from App's
                              descriptions), script_saves (ScriptProgress, SaveSlots in memory or a folder, the slot
                              format), src/scripts/save_api (save.write/read/info/list)
    ui/                       the game's UI as entities (src/ui alone): ui_canvas (on the screen - its height the
                              screen's - or in the world), ui_rect (flexbox by Yoga: lengths "auto", "120", "50%",
                              absolute pinning, direction, justify, align, grow, gap, visible), ui_image (a color or an
                              image, 9-slice), ui_text (a font's MSDF glyphs, wrapped, aligned; measured for layout),
                              ui_button, ui_toggle, ui_slider (tints by state; on_click, on_change as ScriptCalls);
                              UiScene (quads by canvas, a world's service, drawn by the Renderer), UiWorld (a service:
                              SetScreen, Take - input before the game's actions -, boxes, hover, focus; arrows, Tab and
                              a gamepad's pad navigate a canvas with navigation); scripts' ui.focus/focused; laid out
                              in Phase::Ui
    world/                    App (one a process: Jobs, reflection, component types, modules), World (a scene and its
                              modules: data events - added, changed, removed - delivered at the world's turns, the
                              frame's phases in one list, FixedClock, Services by type, Events queued - delivered
                              after each step's physics and before the frame's phases, a paused world's too -;
                              Snapshot/Restore - runtime built again from the data, a module's own state through
                              Module::TrySnapshot/Restoring/Restored -, WorldInput: the actions a step and a frame
                              see, GetActions; World::Over - a world over a scene it does not own, a document's: it
                              leaves the scene its data when it goes -; WorldOptions::mEditing - the modules with
                              ModuleType::mInEditor alone, never a step), Module (ModuleType::mScripts: its
                              ScriptApi); transform (position,
                              rotation in radians - Y, X, Z -, scale; AnglesOf, ComputeWorldMatrix) and
                              TransformModule: WorldTransform each frame, through an entity's ParentOffset (a joint
                              it hangs on) when it has one; ProjectSettings (project.bubble: name, start level,
                              actions; TryReadProjectSettings); Game (a service: what scripts ask of whatever runs
                              the world - game.pause/paused, lock_cursor/cursor_locked, quit -, GameModule)
projects/engine/src/...       same tree; ScriptRuntime's .cpp is spread over src/scripts/runtime/
projects/engine/shaders/      the engine's WGSL: lit (what surface() gets and gives, the vertex stage - vs_skinned for
                              a skin -, PBR lighting, the shadow; built after a material's own), standard_material,
                              shadow, pick (each with a skinned vertex stage too), tonemap, ui (images and MSDF
                              glyphs)
projects/import/include/bubble/import/
                              bubble_import, on the engine and never included by it: ProjectFiles (the project's
                              files: read, list with stamps, a save replaces the file whole, remove, move), ImportFile
                              (<source>.import: the asset's id and settings), TryScanProject (ids from .import files
                              and from inside levels and prefabs, new ones written, a copy re-ided, orphans listed);
                              Importer
                              (source + settings -> the engine's format; mMoreKinds: what else it makes under the
                              same id, written every time, empty when the source has none), the import cache (TryImportAsset,
                              ImportProject: again when the source's hash, the settings' or the importer's version
                              changes), ScriptImporter (.luau -> ScriptAsset: compiled, props
                              and requires read from the AST; with a ScriptCheck - bubble.d.luau, .luaurc aliases,
                              the project's files - checked strictly by Luau.Analysis: no --!nonstrict, no any, the
                              callbacks annotated; TryProjectScriptCheck, TryReadLuaurc), ShaderImporter (.wgsl, the contract checked),
                              MaterialImporter (.mat checked), PrefabImporter (.prefab kept, checked against the
                              component types), AnimationGraphImporter (.anim kept, checked), FontImporter (ttf, otf:
                              FreeType's outlines as msdfgen's fields in one atlas - Latin, Latin-1, Cyrillic and what
                              "chars" adds -, metrics, kerning; not on the web - BUBBLE_IMPORT_FONTS), SoundImporters (wav, flac, mp3: kept, checked by decoding; longer
                              than 10 s streams, or as the .import's "stream" says), TextureImporters (stb_image: png, jpg,
                              tga, bmp -> RGBA8, mips averaged as light), ModelImporters (assimp: glb, gltf, fbx,
                              obj; meshoptimizer's ordering and LODs; a skin - its bones and every node over them -
                              as the model's joints and ozz's skeleton and clips beside it, a mesh with no bones on
                              the joint over it, root motion out of the clips as "root_motion" says - our own, the turn
                              about the model's up by swing-twist, whichever way the file stands up -; not on the web
                              - BUBBLE_IMPORT_MODELS);
                              ProjectAssets (the editor's project kept
                              in step: TryRefresh polls the stamps, scans and imports what changed - a prefab, its id
                              inside, with no .import -, says what the registry reloads); project_setup (what
                              bubble_play and the editor set up alike: AddEngineModules, AddEngineImporters,
                              RegisterEngineLoaders)
projects/editing/include/bubble/editing/
                              bubble_editing, on the engine and import, never included by them: Document (a scene, its
                              History with nested groups, dirty by position, its Selection, the editor's view of it -
                              a DocumentView, gone before its scene), Command, the edits
                              (TrySetField, TryCreateEntity...; a rename or a move keeps links), Clipboard and
                              paste/duplicate; Workspace (open documents, the active one, the catalog it adds the
                              files it writes to - following files moved and removed -, prefabs by id) and Operators -
                              the only way the editor acts ("entity.rename", JSON args, one step each, built-ins in
                              builtin_operators.cpp, prefab_operators.cpp, asset_operators.cpp - asset.create from
                              a kind's template, rename with its .import, delete -); DataDocument (a .mat or .anim
                              open as its JSON with a history of its own, active instead of any scene; data.set,
                              data.remove in data_operators.cpp; saved, undone, closed as a scene's are); prefab
                              documents (.prefab, node ids
                              kept by Entity), a prefab saved remakes the instances open (src/prefabs.cpp);
                              EditorLua (the editor's own VM: editor.ops.*, editor.get, editor.group); undo brings
                              an entity back under its own number (Revive)
projects/editor/              bubble_editor, on editing and Dear ImGui, never included by the rest: ImGui's backends
                              of our own (ImGuiInput - the platform's events in, the pointer, text input and the
                              clipboard out -, ImGuiRenderer - its textures, the font atlas grown as glyphs are needed,
                              and its lists drawn by our GPU; our textures in as TextureId( view ) -), EditorProject
                              (a project open: settings, App with every module, the registry, its sources kept
                              imported, the Workspace and Operators), DocumentWorld (a document's editing world over
                              its scene and the camera on it, its view: ViewOf, WorldOf), MaterialPreview (a
                              material document's view: a sphere wearing it as the document has it, saved or not,
                              in a world of its own; its bases and its shader's parameters followed as they load -
                              TryLayout -, why it does not read), EditorCamera, Viewport (the active document's world in a
                              picture the panel's size, the camera on the mouse - F frames the selection -, a click
                              picks through RenderView::Pick - PickTarget: a prefab's instance whole first -, the
                              gizmo on ImGuizmo moves, turns and scales the active entity - W, E, R, X; one history
                              group a drag -, the selection's boxes, colliders, lights' reach and cameras' views as
                              the world's debug lines; Decompose), ScenePanel (the tree: clicks with
                              ctrl and shift, F2, drag to move, a menu and keys - ClickCall and TryDropCall say what
                              they ask; operators run after the rows are walked; an entity made active elsewhere is
                              shown), Inspector (the active entity: its name, its components' fields as Values by
                              kind - FieldInfo's angle, slider, colour, mAssetKind for the asset picker -, an entity
                              field taking a row dropped from the tree, a script's props from its file - set ones
                              apart -, add, remove, revert to and apply to the prefab; every edit property.set, a
                              drag one history group however many frames - Settle closes it -; a material's base or
                              shader, flags and its shader's parameters - own, its base's or the default; data.set -;
                              TryWhere for tests),
                              AssetsPanel (the project's folders and files - kinds, import errors -, a file dragged
                              out as cAssetPayload to a field, the tree or the viewport, a level or prefab opened at a
                              double click, anything else handed to the system; new assets from templates, rename,
                              delete; TryPlaceAsset: a prefab or a model into the scene, one step), Editor (the dock
                              space, the menu - New Level, Save As -, a tab a document - data documents too -, the
                              scene, the viewport - the active document's world or a material's preview -, the
                              inspector, the assets, the log; operators alone change anything). main.cpp is
                              bubble_editor <project folder> [--open path]... [--frames N] [--screenshot file.png], the
                              desktop alone; its layout in the project's .bubble/editor_layout.ini
projects/runner/              bubble_runner, on the engine alone (never import or editing); for now a scene made in
                              code - boxes of a few materials, one its own WGSL, the sun, a lamp - drawn by the
                              Renderer and played by scripts compiled in memory on physics: the red box - a character
                              controller - walks by move_x and move_z, jumps with a blip, throws crates (a prefab
                              spawned on E), two boxes tumble, the camera follows, a HUD - a button dropping a crate,
                              a slider for the sounds' volume - takes the input over it,
                              a pad chimes and lights up when it is reached (progress, the autosave slot in
                              Platform::TrySaveFolder); a touch or click picking what it lands on;
                              `--frames N` quits after N frames, exit code 0
projects/play/                bubble_play <folder> [--frames N] [--screenshot file.png] [--import-only], on the engine
                              and import, the desktop alone: a project played from its folder - project.bubble's
                              start level and actions, its sources imported into .bubble/imported as it starts and
                              again as they change, the scripts checked against .bubble/bubble.d.luau (written there
                              for a language server too), the Game service's cursor and quit; the sandbox is played
                              with it
projects/tests/               doctest; folders follow the engine's, import/, editing/ and editor/; log_watch.hpp (what was
                              logged); animation/skinned_model.hpp (a skinned .glb made in code: two joints, three
                              clips); ui/test_font.hpp (a TrueType font made in code: a few glyphs, a kerning pair); render/render_fixture.hpp (a project in memory, the GPU, the renderer, frames
                              drawn and read back); render/reference_image.hpp + render/reference/*.png (pictures
                              checked with a rasterizer's tolerance; BUBBLE_UPDATE_REFERENCES=1 writes them anew, a
                              failing one goes beside the tests' binary); deps/ tests deps (luaubind
                              too)
```

## Style

`STYLE.md` is the rule book - read it before writing code. The essentials:

- **Errors.** Nothing there and that is normal: `opt` / `OptRef`. Can fail
  and someone should hear why: `expected<T, E>` with a message a person can
  act on (what is wrong, what there is instead). Cannot happen if the code is
  right: `logic_error`, never caught to carry on; the engine's own
  costly invariants: `BUBBLE_ASSERT`, debug only. Context is added once per
  level on the way up; the code that stops an error logs it, once, and does
  not return it again. No error codes, bool + out-param or null for "failed".
- **Names.** Types and functions `PascalCase`, members `mName`, statics
  `sName`, constants `cName`, files `snake_case`. Everything a user sees -
  components, fields, Luau API, operators, files - is `snake_case`.
- **Formatting.** Four spaces, braces on their own line, spaces inside
  parentheses: `Foo( a, b )`. `and`, `or`, `not`. 120 columns.
- **Build time.** Code goes in the `.cpp` by default; a template goes in a
  header when it has to, and stays there when moving it out would take
  contortions - a balance of reading and compiling.
- **No `std::`.** The standard library comes through `bubble/types`
  (`string`, `vector`, `hmap`, `opt`, `expected`, `unexpected`, `function`,
  `format`, `OsPath`, `Ref`, `u32`...); a missing name is added there as a
  `using`. Left as `std::`: `move`, `forward`, `hash` specializations, the C
  library at a C boundary.
- **Pointers.** Owning: `Scope`/`Ref`; a shared asset: `AssetRef<T>`. A link to something that outlives you:
  `T&`. A link to something that can go away: `Handle` from a `SlotMap`.
  Maybe-missing results and parameters: `OptRef<T>`, never a member. Raw
  pointers only at a C API boundary inside the low layers.
- **`Id` vs `Handle`.** `...Id` is saved (`AssetId`); `...Handle` is a
  run-time link with a generation, never written to a file. A variable that
  links says what and how: `scriptHandle`, `mInstanceHandle`, `assetRef`,
  `assetId`; a bare `handle` only in generic code like `SlotMap`.
- **`Maybe` and `Try`.** A variable, parameter or member of type `opt`, `OptRef`
  or `expected` ends in `Maybe`: `const auto lightMaybe = scene.TryGet<Light>( entity )`.
  A function returning `opt`, `OptRef`, `expected` (or an `AssetRef`/pointer that may
  be empty) is `Try...`: `TryGet`, `TryFind`, `TryFrom`, `TryLoad`; a plain `Get`/`As`
  always has a result (missing is a `logic_error`).
- **Comments** say why, in plain sentences, at the density of the code around
  them. No commented-out code.
- **Strings** are UTF-8 in `string`; identifiers ASCII; never build an
  `OsPath` from a `string` (use `PathFromUtf8`).
- **No globals** besides the log, the profiler and the Name table. **No migration code.**
  **Profiling** through `bubble/core/profile.hpp` macros only.
- **Tests**: doctest, named as sentences, in the folder that mirrors the
  code; error messages checked by their text; Luau tests check the stack is
  balanced (`lua_gettop( L ) == 0`).

## Working rules

- Commit and push only when asked.
- Scripting work: use the `bubble-scripting` skill.
