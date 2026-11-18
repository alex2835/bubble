#include "bubble/import/project_assets.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/import/import_cache.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/import/project_scan.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
ProjectAssets::ProjectAssets( ProjectFiles files, Importers importers, span<const string_view> engineExtensions )
    : mFiles( std::move( files ) ),
      mImporters( std::move( importers ) ),
      mEngineExtensions( engineExtensions.begin(), engineExtensions.end() )
{
}

expected<ProjectRefresh, string> ProjectAssets::TryRefresh()
{
    BUBBLE_PROFILE_ZONE();
    auto listedMaybe = mFiles.TryList();
    if ( not listedMaybe )
        return unexpected( std::move( listedMaybe.error() ) );
    hmap<AssetPath, u64> stamps;
    for ( const ListedFile& file : *listedMaybe )
        stamps.emplace( file.mPath, file.mStamp );
    if ( mStampsMaybe and stamps == *mStampsMaybe )
        return ProjectRefresh{};

    const vector<string_view> sourceExtensions = mImporters.Extensions();
    auto scanMaybe = TryScanProject( mFiles, sourceExtensions, mEngineExtensions );
    if ( not scanMaybe )
        return unexpected( std::move( scanMaybe.error() ) );
    // Assigned, not replaced: the registry and the workspace hold this one.
    mCatalog = std::move( scanMaybe->mCatalog );
    ProjectRefresh refresh{ .mImported = {},
                            .mWarnings = std::move( scanMaybe->mWarnings ),
                            .mErrors = {},
                            .mOrphans = std::move( scanMaybe->mOrphans ) };

    // Changed since the last look, or there was none. An .import the scan
    // wrote just now is new to the next look too: its source is looked at
    // again then, and found up to date.
    const auto changed = [&]( const AssetPath& path ) {
        if ( not mStampsMaybe )
            return true;
        const auto before = mStampsMaybe->find( path );
        const auto now = stamps.find( path );
        return before == mStampsMaybe->end() or now == stamps.end() or before->second != now->second;
    };
    for ( const AssetPath& path : mCatalog.Paths() )
    {
        const auto importerMaybe = mImporters.TryFind( path.Extension() );
        if ( not importerMaybe or not( changed( path ) or changed( ImportFileOf( path ) ) ) )
            continue;
        // An engine file - a prefab - keeps its id inside and has no .import,
        // so no settings either.
        const AssetId assetId = *mCatalog.TryIdOf( path );
        const bool engineFile = ranges::find( mEngineExtensions, path.Extension() ) != mEngineExtensions.end();
        const auto outcomeMaybe = engineFile
                                      ? TryImportAsset( mFiles, *importerMaybe, path, assetId, Json( Json::Object() ) )
                                      : TryImportSource( mFiles, *importerMaybe, path, assetId );
        if ( not outcomeMaybe )
            refresh.mErrors.push_back( outcomeMaybe.error() );
        else if ( *outcomeMaybe == ImportOutcome::Imported )
            refresh.mImported.push_back( path );
    }
    mStampsMaybe = std::move( stamps );
    return refresh;
}
}
