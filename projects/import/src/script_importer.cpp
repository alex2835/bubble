#include "bubble/import/script_importer.hpp"
#include "bubble/scripts/lua.hpp"
#include "bubble/scripts/script_asset.hpp"

namespace bubble
{
Importer ScriptImporter()
{
    return Importer{
        .mExtension = ".luau",
        .mKind = string( cScriptKind ),
        .mVersion = 1,
        .mImport = []( string_view source, const Json&, const AssetPath& path ) -> expected<string, string> {
            auto bytecodeMaybe = TryCompileScript( source );
            if ( not bytecodeMaybe )
                // Luau's message starts with the line: ":3: Expected ...".
                return unexpected( path.String() + bytecodeMaybe.error() );
            return std::move( *bytecodeMaybe );
        },
    };
}
}
