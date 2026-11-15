#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/import/importer.hpp"
#include "bubble/import/project_files.hpp"

namespace bubble
{
// The import cache: what importers made of the project's sources, in
// .bubble/imported - not in git, made again whenever it is missing. Beside
// each <id>.<kind> lies <id>.key, what it was made from: the source's hash,
// the settings' hash and the importer's version. A source is imported again
// when its key changes - the same in the editor as files change and in the
// cook.

enum class ImportOutcome : u8
{
    Imported,
    UpToDate,
};

// Imports the source at `path`, the asset with the id, unless its key is
// the one the cache has. The settings are the .import's. A source the
// importer refuses leaves the cache as it was - the registry keeps the
// version it had - and the error says why.
expected<ImportOutcome, string> TryImportAsset( const ProjectFiles& files,
                                                const Importer& importer,
                                                const AssetPath& path,
                                                const AssetId& assetId,
                                                const Json& settings );

// What a pass over the project imported, and what would not import.
struct ProjectImport
{
    vector<AssetPath> mImported;
    vector<string> mErrors;
};

// Every source in the catalog that has an importer, in the order of paths,
// as TryImportAsset imports it, with the settings in its .import. One that
// fails is a line in mErrors, and the rest go on.
ProjectImport ImportProject( const ProjectFiles& files, const AssetCatalog& catalog, const Importers& importers );
}
