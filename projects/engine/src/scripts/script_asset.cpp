#include "bubble/scripts/script_asset.hpp"
#include "bubble/assets/asset_registry.hpp"

namespace bubble
{
void RegisterScriptLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<ScriptAsset>( cScriptKind, []( string_view bytecode, const AssetPath& ) {
        return expected<ScriptAsset, string>( ScriptAsset{ string( bytecode ) } );
    } );
}
}
