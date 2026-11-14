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

vector<Scope<Document>>::iterator Workspace::Find( const Document& document )
{
    const auto found =
        ranges::find_if( mDocuments, [&]( const Scope<Document>& open ) { return open.get() == &document; } );
    if ( found == mDocuments.end() )
        throw logic_error( "a document this workspace does not have open" );
    return found;
}
}
