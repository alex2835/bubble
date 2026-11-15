#pragma once
#include "bubble/assets/asset_id.hpp"
#include "bubble/core/asset_path.hpp"
#include "bubble/data/json.hpp"

namespace bubble
{
// What lies beside a source file as <its name>.import - hero.glb.import -
// and goes to git with it: the asset's id, which moves with the file when
// it is moved by the editor, by a file manager or by git, and how the file
// is imported.
//
//     {
//       "version": 1,
//       "asset": "9c1e0f3a5b7d4e2c8a6f1b3d5e7c9a0b",
//       "settings": {}
//     }
struct ImportFile
{
    static constexpr int cVersion = 1;

    AssetId mId;
    // As written: each importer reads its own settings from it.
    Json mSettings = Json( Json::Object() );
};

// The error says where in the text and what was there instead.
expected<ImportFile, string> TryParseImportFile( string_view text );
string WriteImportFile( const ImportFile& file );

constexpr string_view cImportExtension = ".import";

// "models/hero.glb.import" for models/hero.glb.
AssetPath ImportFileOf( const AssetPath& source );
// models/hero.glb for "models/hero.glb.import"; nothing for a path that is
// not an import file.
opt<AssetPath> TrySourceOf( const AssetPath& importFile );
}
