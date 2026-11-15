#include "bubble/editing/workspace.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
Document& Workspace::Open( Scene scene )
{
    mDocuments.push_back( CreateScope<Document>( mComponentTypes, std::move( scene ) ) );
    return *mDocuments.back();
}

void Workspace::Close( Document& document )
{
    if ( document.GetHistory().InGroup() )
        throw logic_error( "Workspace::Close of a document an operator is working on" );
    mDocuments.erase( Find( document ) );
}

void Workspace::Activate( Document& document )
{
    const auto found = Find( document );
    Scope<Document> active = std::move( *found );
    mDocuments.erase( found );
    mDocuments.push_back( std::move( active ) );
}

OptRef<Document> Workspace::TryActive()
{
    return mDocuments.empty() ? OptRef<Document>() : OptRef<Document>( *mDocuments.back() );
}

OptRef<const Document> Workspace::TryActive() const
{
    return mDocuments.empty() ? OptRef<const Document>() : OptRef<const Document>( *mDocuments.back() );
}

OptRef<Document> Workspace::TryFind( const AssetPath& path )
{
    for ( const Scope<Document>& document : mDocuments )
        if ( const auto pathMaybe = document->TryPath(); pathMaybe and *pathMaybe == path )
            return *document;
    return nullopt;
}

expected<AssetId, string> Workspace::TryIdOf( const AssetPath& path )
{
    if ( const auto assetIdMaybe = mAssets.TryIdOf( path ) )
        return *assetIdMaybe;
    auto textMaybe = mFiles.TryRead( path );
    if ( not textMaybe )
        return unexpected( std::move( textMaybe.error() ) );
    return Take( path, *textMaybe );
}

AssetId Workspace::IdOf( const AssetPath& path )
{
    if ( const auto assetIdMaybe = mAssets.TryIdOf( path ) )
        return *assetIdMaybe;
    const auto textMaybe = mFiles.TryRead( path );
    return Take( path, textMaybe ? string_view( *textMaybe ) : string_view() );
}

AssetId Workspace::Take( const AssetPath& path, string_view text )
{
    const auto ownMaybe = TryReadFileId( text );
    const bool ownFree = ownMaybe and *ownMaybe and not mAssets.TryPathOf( **ownMaybe );
    const AssetId assetId = ownFree ? **ownMaybe : AssetId::Generate();
    mAssets.Add( assetId, path );
    return assetId;
}

expected<Ref<const Prefab>, string> Workspace::TryPrefab( const AssetId& prefabId )
{
    if ( const auto known = mPrefabs.find( prefabId ); known != mPrefabs.end() )
        return known->second;
    const auto pathMaybe = mAssets.TryPathOf( prefabId );
    if ( not pathMaybe )
        return unexpected( format( "no asset {} in the project", prefabId.ToString() ) );
    auto textMaybe = mFiles.TryRead( *pathMaybe );
    if ( not textMaybe )
        return unexpected( std::move( textMaybe.error() ) );
    auto prefabMaybe = Prefab::TryParse( *textMaybe, FileContext( mComponentTypes, mAssets ) );
    if ( not prefabMaybe )
        return unexpected( std::move( prefabMaybe.error() ) );
    Ref<const Prefab> prefab = CreateRef<const Prefab>( std::move( *prefabMaybe ) );
    mPrefabs[prefabId] = prefab;
    return prefab;
}

FileContext Workspace::GetFileContext()
{
    return FileContext( mComponentTypes, mAssets, [this]( const AssetId& prefabId ) { return TryPrefab( prefabId ); } );
}

vector<Scope<Document>>::iterator Workspace::Find( const Document& document )
{
    const auto found =
        ranges::find_if( mDocuments, [&]( const Scope<Document>& open ) { return open.get() == &document; } );
    if ( found == mDocuments.end() )
        throw logic_error( "a document this workspace does not have open" );
    return found;
}
}
