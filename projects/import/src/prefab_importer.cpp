#include "bubble/import/prefab_importer.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/scene/prefab.hpp"

namespace bubble
{
Importer PrefabImporter( const ComponentTypes& componentTypes, const AssetCatalog& catalog )
{
    return Importer{
        .mExtension = ".prefab",
        .mKind = string( cPrefabKind ),
        .mVersion = 1,
        .mImport = [&componentTypes, &catalog]( string_view source, const Json&,
                                                const AssetPath& path ) -> expected<string, string> {
            if ( auto prefabMaybe = Prefab::TryParse( source, FileContext( componentTypes, catalog ) );
                 not prefabMaybe )
                return unexpected( path.String() + ": " + prefabMaybe.error() );
            return string( source );
        },
    };
}
}
