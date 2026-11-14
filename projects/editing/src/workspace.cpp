#include "bubble/editing/workspace.hpp"
#include "bubble/types/algorithm.hpp"

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

expected<Ref<const Prefab>, string> Workspace::TryPrefab( const AssetPath& path )
{
    if ( const auto known = mPrefabs.find( path ); known != mPrefabs.end() )
        return known->second;
    auto textMaybe = mFiles.TryRead( path );
    if ( not textMaybe )
        return unexpected( std::move( textMaybe.error() ) );
    auto prefabMaybe = Prefab::TryParse( *textMaybe, mComponentTypes );
    if ( not prefabMaybe )
        return unexpected( std::move( prefabMaybe.error() ) );
    Ref<const Prefab> prefab = CreateRef<const Prefab>( std::move( *prefabMaybe ) );
    mPrefabs[path] = prefab;
    return prefab;
}

PrefabSource Workspace::Prefabs()
{
    return [this]( const AssetPath& path ) { return TryPrefab( path ); };
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
