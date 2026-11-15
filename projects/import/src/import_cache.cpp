#include "bubble/import/import_cache.hpp"
#include "bubble/assets/asset_registry.hpp"
#include "bubble/core/profile.hpp"
#include "bubble/import/import_file.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
namespace
{
// FNV-1a, 64 bits: whether a file changed, not a guard against anyone.
u64 Hash( string_view bytes )
{
    u64 hash = 0xcbf29ce484222325ull;
    for ( const char c : bytes )
    {
        hash ^= static_cast<u8>( c );
        hash *= 0x100000001b3ull;
    }
    return hash;
}

AssetPath KeyPath( const AssetId& assetId )
{
    return ImportedPath( assetId, "key" );
}
}

expected<ImportOutcome, string> TryImportAsset( const ProjectFiles& files,
                                                const Importer& importer,
                                                const AssetPath& path,
                                                const AssetId& assetId,
                                                const Json& settings )
{
    BUBBLE_PROFILE_ZONE();
    auto sourceMaybe = files.TryRead( path );
    if ( not sourceMaybe )
        return unexpected( format( "{}: {}", path.View(), sourceMaybe.error() ) );
    const string key = format( "{} {} {:016x} {:016x}\n", importer.mKind, importer.mVersion, Hash( *sourceMaybe ),
                               Hash( settings.Write() ) );
    if ( files.TryRead( KeyPath( assetId ) ) == key )
        return ImportOutcome::UpToDate;

    auto importedMaybe = importer.mImport( *sourceMaybe, settings, path );
    if ( not importedMaybe )
        return unexpected( std::move( importedMaybe.error() ) );
    // What it made first, then the key: a pass cut short in between imports
    // it again next time.
    const AssetPath importedPath = ImportedPath( assetId, importer.mKind );
    if ( auto writtenMaybe = files.TryWrite( importedPath, *importedMaybe ); not writtenMaybe )
        return unexpected(
            format( "{}: cannot write {}: {}", path.View(), importedPath.View(), writtenMaybe.error() ) );
    if ( auto writtenMaybe = files.TryWrite( KeyPath( assetId ), key ); not writtenMaybe )
        return unexpected(
            format( "{}: cannot write {}: {}", path.View(), KeyPath( assetId ).View(), writtenMaybe.error() ) );
    return ImportOutcome::Imported;
}

ProjectImport ImportProject( const ProjectFiles& files, const AssetCatalog& catalog, const Importers& importers )
{
    BUBBLE_PROFILE_ZONE();
    ProjectImport result;
    for ( const AssetPath& path : catalog.Paths() )
    {
        const auto importerMaybe = importers.TryFind( path.Extension() );
        if ( not importerMaybe )
            continue;
        const AssetPath importPath = ImportFileOf( path );
        auto textMaybe = files.TryRead( importPath );
        if ( not textMaybe )
        {
            result.mErrors.push_back( format( "{}: {}", importPath.View(), textMaybe.error() ) );
            continue;
        }
        const auto importFileMaybe = TryParseImportFile( *textMaybe );
        if ( not importFileMaybe )
        {
            result.mErrors.push_back( format( "{}: {}", importPath.View(), importFileMaybe.error() ) );
            continue;
        }
        const auto outcomeMaybe =
            TryImportAsset( files, *importerMaybe, path, *catalog.TryIdOf( path ), importFileMaybe->mSettings );
        if ( not outcomeMaybe )
            result.mErrors.push_back( outcomeMaybe.error() );
        else if ( *outcomeMaybe == ImportOutcome::Imported )
            result.mImported.push_back( path );
    }
    return result;
}
}
