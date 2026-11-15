#include "bubble/import/project_scan.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/scene/engine_file.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
expected<string, string> TryReadListed( const ProjectFiles& files, const AssetPath& path )
{
    auto bytesMaybe = files.TryRead( path );
    if ( not bytesMaybe )
        return unexpected( format( "cannot read {}: {}", path.View(), bytesMaybe.error() ) );
    return bytesMaybe;
}

expected<void, string> TryWriteImportFile( const ProjectFiles& files, const AssetPath& path, const ImportFile& file )
{
    if ( auto writtenMaybe = files.TryWrite( path, WriteImportFile( file ) ); not writtenMaybe )
        return unexpected( format( "cannot write {}: {}", path.View(), writtenMaybe.error() ) );
    return {};
}
}

expected<ProjectScan, string> TryScanProject( const ProjectFiles& files,
                                              span<const string_view> sourceExtensions,
                                              span<const string_view> engineExtensions )
{
    auto pathsMaybe = files.TryList();
    if ( not pathsMaybe )
        return unexpected( std::move( pathsMaybe.error() ) );
    const vector<AssetPath>& paths = *pathsMaybe;
    const auto listed = [&]( const AssetPath& path ) { return ranges::binary_search( paths, path ); };
    const auto isSource = [&]( const AssetPath& path ) {
        return ranges::find( sourceExtensions, path.Extension() ) != sourceExtensions.end();
    };
    const auto isEngineFile = [&]( const AssetPath& path ) {
        return ranges::find( engineExtensions, path.Extension() ) != engineExtensions.end();
    };

    ProjectScan scan;
    for ( const AssetPath& path : paths )
    {
        if ( isEngineFile( path ) )
        {
            auto textMaybe = TryReadListed( files, path );
            if ( not textMaybe )
                return unexpected( std::move( textMaybe.error() ) );
            const auto ownMaybe = TryReadFileId( *textMaybe );
            if ( not ownMaybe )
            {
                scan.mWarnings.push_back(
                    format( "{}: {} - it is left out until it is mended", path.View(), ownMaybe.error() ) );
                continue;
            }
            AssetId assetId = ownMaybe->value_or( AssetId() );
            const auto otherMaybe = assetId ? scan.mCatalog.TryPathOf( assetId ) : nullopt;
            if ( otherMaybe )
                scan.mWarnings.push_back(
                    format( "{} had the id of {} - a copy? - and has a new one", path.View(), otherMaybe->View() ) );
            if ( not assetId or otherMaybe )
            {
                assetId = AssetId::Generate();
                auto withIdMaybe = TryWithFileId( *textMaybe, assetId );
                if ( not withIdMaybe )
                    return unexpected( format( "{}: {}", path.View(), withIdMaybe.error() ) );
                if ( auto writtenMaybe = files.TryWrite( path, *withIdMaybe ); not writtenMaybe )
                    return unexpected( format( "cannot write {}: {}", path.View(), writtenMaybe.error() ) );
            }
            scan.mCatalog.Add( assetId, path );
        }
        else if ( isSource( path ) )
        {
            const AssetPath importPath = ImportFileOf( path );
            if ( not listed( importPath ) )
            {
                ImportFile made;
                made.mId = AssetId::Generate();
                if ( auto writtenMaybe = TryWriteImportFile( files, importPath, made ); not writtenMaybe )
                    return unexpected( std::move( writtenMaybe.error() ) );
                scan.mCatalog.Add( made.mId, path );
                continue;
            }
            auto bytesMaybe = TryReadListed( files, importPath );
            if ( not bytesMaybe )
                return unexpected( std::move( bytesMaybe.error() ) );
            auto importFileMaybe = TryParseImportFile( *bytesMaybe );
            if ( not importFileMaybe )
            {
                scan.mWarnings.push_back( format( "{}: {} - {} is left out until it is mended", importPath.View(),
                                                  importFileMaybe.error(), path.View() ) );
                continue;
            }
            if ( const auto otherMaybe = scan.mCatalog.TryPathOf( importFileMaybe->mId ) )
            {
                scan.mWarnings.push_back( format( "{} had the id of {} - copied with its .import? - and has a new one",
                                                  path.View(), otherMaybe->View() ) );
                importFileMaybe->mId = AssetId::Generate();
                if ( auto writtenMaybe = TryWriteImportFile( files, importPath, *importFileMaybe ); not writtenMaybe )
                    return unexpected( std::move( writtenMaybe.error() ) );
            }
            scan.mCatalog.Add( importFileMaybe->mId, path );
        }
        else if ( const auto sourceMaybe = TrySourceOf( path ); sourceMaybe and not listed( *sourceMaybe ) )
        {
            auto bytesMaybe = TryReadListed( files, path );
            if ( not bytesMaybe )
                return unexpected( std::move( bytesMaybe.error() ) );
            const auto importFileMaybe = TryParseImportFile( *bytesMaybe );
            if ( not importFileMaybe )
            {
                scan.mWarnings.push_back( format( "{}: {}", path.View(), importFileMaybe.error() ) );
                continue;
            }
            scan.mOrphans.emplace_back( path, importFileMaybe->mId );
        }
    }
    return scan;
}
}
