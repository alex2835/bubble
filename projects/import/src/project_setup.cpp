#include "bubble/import/project_setup.hpp"
#include "bubble/animation/animation.hpp"
#include "bubble/animation/animation_assets.hpp"
#include "bubble/assets/font_asset.hpp"
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/import/animation_graph_importer.hpp"
#include "bubble/import/font_importer.hpp"
#include "bubble/import/material_importers.hpp"
#include "bubble/import/model_importer.hpp"
#include "bubble/import/prefab_importer.hpp"
#include "bubble/import/sound_importers.hpp"
#include "bubble/import/texture_importer.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scripts/script_asset.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/game.hpp"
#include "bubble/world/transform.hpp"

namespace bubble
{
void AddEngineModules( App& app, AssetRegistry& assets, AudioDevice& audio, ScriptModuleOptions scripts )
{
    app.AddModule( TransformModule() );
    app.AddModule( ScriptModule( assets, std::move( scripts ) ) );
    app.AddModule( PhysicsModule( assets ) );
    app.AddModule( AudioModule( assets, audio ) );
    app.AddModule( AnimationModule( assets ) );
    app.AddModule( UiModule( assets ) );
    app.AddModule( RenderModule( assets ) );
    app.AddModule( GameModule() );
}

void AddEngineImporters( ProjectAssets& project, const App& app, ScriptCheck check )
{
    // Models and fonts are imported where their libraries are built: not on the web.
    vector<vector<Importer>> groups{ TextureImporters(), SoundImporters() };
#ifdef BUBBLE_IMPORT_MODELS
    groups.push_back( ModelImporters() );
#endif
    for ( vector<Importer>& group : groups )
        for ( Importer& importer : group )
            project.AddImporter( std::move( importer ) );
    project.AddImporter( ShaderImporter() );
    project.AddImporter( MaterialImporter() );
    project.AddImporter( PrefabImporter( app.GetComponentTypes(), project.GetCatalog() ) );
    project.AddImporter( ScriptImporter( std::move( check ) ) );
    project.AddImporter( AnimationGraphImporter() );
#ifdef BUBBLE_IMPORT_FONTS
    project.AddImporter( FontImporter( ".ttf" ) );
    project.AddImporter( FontImporter( ".otf" ) );
#endif
}

void RegisterEngineLoaders( AssetRegistry& assets )
{
    RegisterModelLoader( assets );
    RegisterMaterialLoader( assets );
    RegisterShaderLoader( assets );
    RegisterTextureLoader( assets );
    RegisterScriptLoader( assets );
    RegisterPrefabLoader( assets );
    RegisterSoundLoader( assets );
    RegisterAnimationLoaders( assets );
    RegisterFontLoader( assets );
}
}
