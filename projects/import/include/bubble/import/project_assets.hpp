#pragma once
#include "bubble/assets/asset_catalog.hpp"
#include "bubble/import/importer.hpp"
#include "bubble/import/project_files.hpp"

namespace bubble
{
// What a look over the project did.
struct ProjectRefresh
{
    // Imported again, or for the first time: what the registry reloads.
    vector<AssetPath> mImported;
    // The scan's warnings and what would not import, a line each, for a
    // person.
    vector<string> mWarnings;
    vector<string> mErrors;
    // Import files whose source is gone, and their ids.
    vector<pair<AssetPath, AssetId>> mOrphans;
};

// The project the editor has open, kept in step with its files as people
// change them - in the editor, in a file manager, in git: the catalog right,
// the sources imported. The editor looks often - TryRefresh is cheap when
// nothing changed - and has the registry reload what was imported:
//
//     const auto refreshMaybe = project.TryRefresh();
//     for ( const AssetPath& path : refreshMaybe->mImported )
//         assets.TryReload( path );
//
// It looks by polling the list of files and their stamps; the system's
// notices of changes could tell it when to look, later.
class ProjectAssets
{
public:
    // `engineExtensions` outlive it: cEngineFileExtensions.
    ProjectAssets( ProjectFiles files, Importers importers, span<const string_view> engineExtensions );
    ProjectAssets( const ProjectAssets& ) = delete;
    ProjectAssets& operator=( const ProjectAssets& ) = delete;

    // Lists the project's files. When none came, went or was written since
    // the last look, nothing more. Else it scans - the catalog follows the
    // files that moved, came and went - and imports the sources that
    // changed and those whose .import did; the first look, every source,
    // as the cache needs. Failing to list or scan the project is the error;
    // a source that will not import is a line in mErrors.
    expected<ProjectRefresh, string> TryRefresh();
    // One more, before the first refresh: an importer that needs the
    // catalog - prefabs, scripts - is made after the catalog is.
    void AddImporter( Importer importer ) { mImporters.Add( std::move( importer ) ); }

    // The same object for as long as this lives - the registry and the
    // workspace hold it - with the entries a refresh finds.
    AssetCatalog& GetCatalog() { return mCatalog; }
    const AssetCatalog& GetCatalog() const { return mCatalog; }
    const ProjectFiles& GetFiles() const { return mFiles; }
    const Importers& GetImporters() const { return mImporters; }

private:
    ProjectFiles mFiles;
    Importers mImporters;
    vector<string_view> mEngineExtensions;
    AssetCatalog mCatalog;
    // As the last look listed them; none before the first.
    opt<hmap<AssetPath, u64>> mStampsMaybe;
};
}
