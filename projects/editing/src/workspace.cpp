#include "bubble/editing/workspace.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
Document& Workspace::Open( Scene scene )
{
    mDocuments.push_back( CreateScope<Document>( mComponentTypes, std::move( scene ) ) );
    mDataActive = false;
    return *mDocuments.back();
}

void Workspace::Close( Document& document )
{
    if ( document.GetHistory().InGroup() )
        throw logic_error( "Workspace::Close of a document an operator is working on" );
    mDocuments.erase( Find( document ) );
    if ( mDocuments.empty() and not mData.empty() )
        mDataActive = true;
}

void Workspace::Activate( Document& document )
{
    const auto found = Find( document );
    Scope<Document> active = std::move( *found );
    mDocuments.erase( found );
    mDocuments.push_back( std::move( active ) );
    mDataActive = false;
}

OptRef<Document> Workspace::TryActive()
{
    return mDocuments.empty() or mDataActive ? OptRef<Document>() : OptRef<Document>( *mDocuments.back() );
}

OptRef<const Document> Workspace::TryActive() const
{
    return mDocuments.empty() or mDataActive ? OptRef<const Document>() : OptRef<const Document>( *mDocuments.back() );
}

OptRef<Document> Workspace::TryShownScene()
{
    return mDocuments.empty() ? OptRef<Document>() : OptRef<Document>( *mDocuments.back() );
}

OptRef<const Document> Workspace::TryShownScene() const
{
    return mDocuments.empty() ? OptRef<const Document>() : OptRef<const Document>( *mDocuments.back() );
}

DataDocument& Workspace::OpenData( AssetPath path, Json data )
{
    mData.push_back( CreateScope<DataDocument>( std::move( path ), std::move( data ) ) );
    mDataActive = true;
    return *mData.back();
}

void Workspace::Close( DataDocument& document )
{
    if ( document.InGroup() )
        throw logic_error( "Workspace::Close of a document an edit is working on" );
    const auto found =
        ranges::find_if( mData, [&]( const Scope<DataDocument>& open ) { return open.get() == &document; } );
    if ( found == mData.end() )
        throw logic_error( "Workspace::Close of a data document not open here" );
    mData.erase( found );
    if ( mData.empty() )
        mDataActive = false;
}

void Workspace::Activate( DataDocument& document )
{
    const auto found =
        ranges::find_if( mData, [&]( const Scope<DataDocument>& open ) { return open.get() == &document; } );
    if ( found == mData.end() )
        throw logic_error( "Workspace::Activate of a data document not open here" );
    Scope<DataDocument> active = std::move( *found );
    mData.erase( found );
    mData.push_back( std::move( active ) );
    mDataActive = true;
}

OptRef<DataDocument> Workspace::TryActiveData()
{
    return mData.empty() or not mDataActive ? OptRef<DataDocument>() : OptRef<DataDocument>( *mData.back() );
}

OptRef<const DataDocument> Workspace::TryActiveData() const
{
    return mData.empty() or not mDataActive ? OptRef<const DataDocument>()
                                            : OptRef<const DataDocument>( *mData.back() );
}

OptRef<DataDocument> Workspace::TryFindData( const AssetPath& path )
{
    for ( const Scope<DataDocument>& document : mData )
        if ( document->GetPath() == path )
            return *document;
    return nullopt;
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

void Workspace::Moved( const AssetPath& from, const AssetPath& to )
{
    if ( const auto assetIdMaybe = mAssets.TryIdOf( from ) )
    {
        mAssets.Remove( *assetIdMaybe );
        mAssets.Add( *assetIdMaybe, to );
    }
    if ( const auto documentMaybe = TryFind( from ) )
        documentMaybe->SetPath( to );
    if ( const auto dataMaybe = TryFindData( from ) )
        dataMaybe->SetPath( to );
}

void Workspace::Removed( const AssetPath& path )
{
    if ( const auto assetIdMaybe = mAssets.TryIdOf( path ) )
        mAssets.Remove( *assetIdMaybe );
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
