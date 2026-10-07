#include "bubble/scripts/script_asset.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/scripts/lua.hpp"

namespace bubble
{
void RegisterScriptImporter( AssetRegistry& registry )
{
    registry.RegisterImporter<ScriptAsset>(
        ".luau", "script", []( string_view source, const AssetPath& path ) -> expected<ScriptAsset, string> {
            auto bytecode = CompileScript( source );
            if ( not bytecode )
                // Luau's message starts with the line: ":3: Expected ...".
                return unexpected( path.String() + bytecode.error() );
            return ScriptAsset{ std::move( *bytecode ) };
        } );
}
}
