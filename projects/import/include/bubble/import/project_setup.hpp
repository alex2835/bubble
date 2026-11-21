#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/world/app.hpp"

// What the tools that open a project's folder - bubble_runner, the editor - set
// up alike, so the two never differ in what a project is.
namespace bubble
{
class AudioDevice;

// Every module the engine has, in the frame's order of their work:
// transforms, scripts, physics, sound, animation, the UI, rendering, the
// game service. The registry and the device outlive the App's worlds.
void AddEngineModules( App& app, AssetRegistry& assets, AudioDevice& audio, ScriptModuleOptions scripts );

// Every importer the engine has, for the project's sources: textures,
// models, sounds, shaders, materials, prefabs, scripts - checked as `check`
// says -, animation graphs, fonts. The App's modules are added already: its
// component types are what prefabs are checked against.
void AddEngineImporters( ProjectAssets& project, const App& app, ScriptCheck check );

// Every loader the engine has, for what the importers wrote.
void RegisterEngineLoaders( AssetRegistry& assets );
}
