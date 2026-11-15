#include "bubble/assets/asset_catalog.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
void AssetCatalog::Add( const AssetId& assetId, const AssetPath& path )
{
    if ( const auto pathMaybe = TryPathOf( assetId ) )
        throw logic_error( format( "AssetCatalog::Add of {} for {}: it is {} already", assetId.ToString(), path.View(),
                                   pathMaybe->View() ) );
    if ( mIds.contains( path ) )
        throw logic_error( format( "AssetCatalog::Add of {}: the path has an asset already", path.View() ) );
    mPaths.emplace( assetId, path );
    mIds.emplace( path, assetId );
}

void AssetCatalog::Remove( const AssetId& assetId )
{
    const auto found = mPaths.find( assetId );
    if ( found == mPaths.end() )
        return;
    mIds.erase( found->second );
    mPaths.erase( found );
}

OptRef<const AssetPath> AssetCatalog::TryPathOf( const AssetId& assetId ) const
{
    const auto found = mPaths.find( assetId );
    if ( found == mPaths.end() )
        return nullopt;
    return found->second;
}

string AssetCatalog::Describe( const AssetId& assetId ) const
{
    if ( const auto pathMaybe = TryPathOf( assetId ) )
        return pathMaybe->String();
    return format( "asset {}", assetId.ToString() );
}

opt<AssetId> AssetCatalog::TryIdOf( const AssetPath& path ) const
{
    const auto found = mIds.find( path );
    if ( found == mIds.end() )
        return nullopt;
    return found->second;
}
}
