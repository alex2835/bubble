#include "bubble/editing/clipboard.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/types/format.hpp"

namespace bubble
{
expected<void, string> Clipboard::TryCopy( const Document& document, span<const Entity> entities )
{
    const Scene& scene = document.GetScene();
    // A copy in memory keeps its assets' ids; their paths are for files.
    const AssetCatalog none;
    const FileContext files( document.GetComponentTypes(), none );
    vector<CopiedEntity> copied;
    for ( const Entity entity : entities )
    {
        if ( not InTree( scene, entity ) )
            return unexpected( format( "not an entity of the document: {}", Describe( scene, entity ) ) );
        if ( entity == scene.Root() )
            return unexpected( "the root is not copied: it is the scene; copy what is under it"s );
        auto jsonMaybe = TryEntityToJson( scene, entity, files );
        if ( not jsonMaybe )
            return unexpected( std::move( jsonMaybe.error() ) );
        copied.push_back( CopiedEntity{ .mEntity = std::move( *jsonMaybe ), .mFrom = PathOf( scene, entity ) } );
    }
    mCopied = std::move( copied );
    return {};
}
}
