#include "bubble/import/importer.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
void Importers::Add( Importer importer )
{
    if ( TryFind( importer.mExtension ) )
        throw logic_error( format( "Importers::Add of a second importer for {}", importer.mExtension ) );
    mImporters.push_back( std::move( importer ) );
}

void Importers::Replace( Importer importer )
{
    const auto found = ranges::find( mImporters, importer.mExtension, &Importer::mExtension );
    if ( found == mImporters.end() )
        throw logic_error( format( "Importers::Replace of an importer for {}, which has none", importer.mExtension ) );
    *found = std::move( importer );
}

OptRef<const Importer> Importers::TryFind( string_view extension ) const
{
    for ( const Importer& importer : mImporters )
        if ( importer.mExtension == extension )
            return importer;
    return nullopt;
}

vector<string_view> Importers::Extensions() const
{
    vector<string_view> extensions;
    for ( const Importer& importer : mImporters )
        extensions.push_back( importer.mExtension );
    return extensions;
}
}
