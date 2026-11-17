#include "bubble/import/animation_graph_importer.hpp"
#include "bubble/animation/animation_graph.hpp"

namespace bubble
{
Importer AnimationGraphImporter()
{
    return Importer{
        .mExtension = ".anim",
        .mKind = string( cAnimationGraphKind ),
        .mVersion = 1,
        .mImport = []( string_view source, const Json&, const AssetPath& path ) -> expected<string, string> {
            if ( auto graphMaybe = TryParseAnimationGraph( source ); not graphMaybe )
                return unexpected( path.String() + ":" + graphMaybe.error() );
            return string( source );
        },
    };
}
}
