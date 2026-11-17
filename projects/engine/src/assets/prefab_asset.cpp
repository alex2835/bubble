#include "bubble/assets/prefab_asset.hpp"
#include "bubble/assets/asset_registry.hpp"

namespace bubble
{
void RegisterPrefabLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<PrefabAsset>( cPrefabKind, []( string_view bytes, const AssetPath& ) {
        return expected<PrefabAsset, string>( PrefabAsset{ string( bytes ) } );
    } );
}
}
