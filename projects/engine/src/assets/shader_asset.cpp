#include "bubble/assets/shader_asset.hpp"
#include "bubble/assets/asset_registry.hpp"

namespace bubble
{
void RegisterShaderLoader( AssetRegistry& registry )
{
    registry.RegisterLoader<ShaderAsset>( cShaderKind, []( string_view source, const AssetPath& ) {
        return expected<ShaderAsset, string>( ShaderAsset{ string( source ) } );
    } );
}
}
