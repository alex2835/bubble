#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/import/project_files.hpp"

namespace bubble
{
// What a look over the project found.
struct ProjectScan
{
    // Every source file the scan took, by its id.
    AssetCatalog mCatalog;
    // What a person should hear of, one line each: a file given a new id
    // because another had its id, an import file that could not be read.
    vector<string> mWarnings;
    // Import files whose source is gone, and the ids they hold: links to
    // those assets are broken until the source is back or the links are
    // pointed elsewhere.
    vector<pair<AssetPath, AssetId>> mOrphans;
};

// Goes over the project's files. A source file - one whose extension is in
// `sourceExtensions` - is the asset with the id in the .import beside it.
// One with no .import gets a new id, and its .import is written. One whose
// id another file had before it, in the order of paths - a file copied with
// its .import - gets a new id too, its .import rewritten, and a warning. One
// whose .import cannot be read is left out, with a warning, and the .import
// stays as it is for a person to mend. Failing to list the files, to read
// one listed or to write an .import fails the scan.
expected<ProjectScan, string> TryScanProject( const ProjectFiles& files, span<const string_view> sourceExtensions );
}
