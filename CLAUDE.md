# Bubble

A small 3D engine rebuilt from the old one (`E:\projects\bubble0.7`, kept
until parity) a stage at a time. Decisions live in the architecture doc,
"Bubble: архитектура движка":
https://claude.ai/code/artifact/cfe42e9b-bcd3-4e0b-bed7-338f80567534 -
read the section you touch before changing a design, and update the doc when
a decision changes. It also holds the stage plan: a feature waits for its
stage, however ready it looks in the old code.

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
deps/                         vendored, each with its license; listed in THIRD_PARTY_NOTICES.md. wgpu-native alone is
                              downloaded at configure time into out/downloads, pinned by version and SHA-256
deps/luaubind/                our own small sol-like C++ binding for Luau; depends on Luau only
deps/wgsl_reflect/            our own reflection of WGSL source: structs laid out for uniforms, bindings, functions,
                              the comment on a line; the standard library only
projects/engine/include/bubble/
    types/                    aliases (std, glm; matrix.hpp for mat4/quat), Handle/SlotMap, OptRef
    core/                     log, profile, utf8, AssetPath, process, Name, Text, Color, Random (PCG32), Jobs (the
                              process's one pool on enkiTS: ParallelFor, Background, RunOnMain; the web and Jobs( 1 ):
                              the main thread alone)
    assets/                   AssetId (random, 32 hex), AssetCatalog (id <-> path), AssetRegistry (entries by id,
                              paths through the catalog, loaders by kind reading the import cache - ImportedPath,
                              .bubble/imported/<id>.<kind> - on Jobs' pool (Load: Loading until the main queue
                              runs, then OnLoaded) or here and now (LoadNow); reads hand their bytes over at once
                              (AtOnce, FromDirectory) or later (FromFetch on the web), AssetRef<T>, reload events);
                              TextureAsset (RGBA8 mip levels, its bytes "BTEX", RegisterTextureLoader), ModelAsset
                              (meshes: vertices, LOD levels sharing them, the box; "BMDL", RegisterModelLoader);
                              MakeBox/MakePlane (models made in code); ShaderAsset (a material's WGSL), MaterialAsset
                              (.mat JSON: shader or base, double_sided, transparent, params by name)
    input/                    controls (Key by USB HID usage - SDL's scancodes -, mouse and gamepad buttons, axes; their
                              snake_case names), InputEvent (the engine's own: keys, mouse, touches as shares of the
                              window, gamepads), ActionMap (actions by name bound to keys, buttons, axes with a scale,
                              touch areas), Input (the devices' state -> actions; LatchStep/LatchFrame each see a press
                              once, at any frame rate)
    platform/                 Platform (SDL, seen nowhere else: one a process, Poll -> InputEvents, gamepads opened as
                              they come, QuitRequested, Run - a loop, or the browser's frames), Window
    render/gpu/               Gpu (WebGPU's device: wgpu-native on the desktop, emdawnwebgpu on the web; Start ->
                              GpuStartup::TryFinish each frame, since a browser answers between frames; TryStartNow
                              for tests; Check/TryCheckNow - validation errors caught, WGSL's with their lines;
                              Submit, Clear, TryReadRgba8), GpuSurface (the window's swap chain, sRGB views,
                              Resize/TryAcquire/Present), GpuHandle (WGPU objects owned; aliases ...Handle),
                              GpuBuffer, GpuTexture (a texture and its view; from a TextureAsset with its mips),
                              CreateShader/CreateRenderPipeline, GpuModel (a ModelAsset's meshes in one vertex and
                              one index buffer, LODs as index ranges), GpuAssets (the GPU's copies of the
                              registry's models and textures by id, made again after a reload, Collect),
                              Renderer (a RenderScene through a camera into a RenderTarget: shadow map, opaque then
                              transparent surfaces in rgba16float with reversed depth, ACES tonemapping; materials
                              built from their bases, shader and textures in src/render/gpu/gpu_materials - the
                              standard one while they load, a magenta placeholder when they fail), EngineShader (the WGSL of
                              projects/engine/shaders, built in by cmake/embed_shaders.cmake); WGPU types stop here
    render/                   render_scene: camera, directional_light, point_light, spot_light, model (asset,
                              materials by mesh, tint); RenderModule gathers them each frame into RenderScene (items,
                              lights, cameras, time), a world's service; material_layout: TryReflectMaterial - what a
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
                              found by the path when the id is not)
    reflection/               Reflection (an entt::meta context): TypeBuilder, FieldInfo, Get/Set/Call by name,
                              FieldPath ("shadow.bias", "steps[2]"), Value <-> MetaAny, field listeners
    scripts/                  lua.hpp (luaubind names in bubble), ScriptAsset, ScriptRuntime
    world/                    App (one a process: Jobs, reflection, component types, modules), World (a scene and its
                              modules: data events - added, changed, removed - delivered at the world's turns, the
                              frame's phases in one list, FixedClock, Services by type, Events queued;
                              Snapshot/Restore - runtime built again from the data), Module; transform (position,
                              rotation in radians - Y, X, Z -, scale) and TransformModule: WorldTransform each frame
projects/engine/src/...       same tree; ScriptRuntime's .cpp is spread over src/scripts/runtime/
projects/engine/shaders/      the engine's WGSL: lit (what surface() gets and gives, the vertex stage, PBR lighting,
                              the shadow; built after a material's own), standard_material, shadow, tonemap
projects/import/include/bubble/import/
                              bubble_import, on the engine and never included by it: ProjectFiles (the project's
                              files: read, list with stamps, a save replaces the file whole), ImportFile
                              (<source>.import: the asset's id and settings), TryScanProject (ids from .import files
                              and from inside levels and prefabs, new ones written, a copy re-ided, orphans listed);
                              Importer
                              (source + settings -> the engine's format), the import cache (TryImportAsset,
                              ImportProject: again when the source's hash, the settings' or the importer's version
                              changes), ScriptImporter (.luau -> bytecode), ShaderImporter (.wgsl, the contract checked),
                              MaterialImporter (.mat checked), TextureImporters (stb_image: png, jpg,
                              tga, bmp -> RGBA8, mips averaged as light), ModelImporters (assimp: glb, gltf, fbx,
                              obj; meshoptimizer's ordering and LODs; not on the web - BUBBLE_IMPORT_MODELS);
                              ProjectAssets (the editor's project kept
                              in step: TryRefresh polls the stamps, scans and imports what changed, says what the
                              registry reloads)
projects/editing/include/bubble/editing/
                              bubble_editing, on the engine and import, never included by them: Document (a scene, its
                              History with nested groups, dirty by position, its Selection), Command, the edits
                              (TrySetField, TryCreateEntity...; a rename or a move keeps links), Clipboard and
                              paste/duplicate; Workspace (open documents, the active one, the catalog it adds the
                              files it writes to, prefabs by id) and Operators - the
                              only way the editor acts ("entity.rename", JSON args, one step each, built-ins in
                              builtin_operators.cpp, prefab_operators.cpp); prefab documents (.prefab, node ids
                              kept by Entity), a prefab saved remakes the instances open (src/prefabs.cpp);
                              EditorLua (the editor's own VM: editor.ops.*, editor.get, editor.group); undo brings
                              an entity back under its own number (Revive)
projects/runner/              bubble_runner, on the engine alone (never import or editing); for now a scene made in
                              code - boxes of a few materials, one its own WGSL, the sun, a lamp - drawn by the
                              Renderer, the camera turned by move_x
projects/tests/               doctest; folders follow the engine's, import/ and editing/; log_watch.hpp (what was
                              logged); deps/ tests deps (luaubind
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
