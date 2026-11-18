#include "bubble/editor/document_world.hpp"

namespace bubble
{
DocumentWorld::DocumentWorld( App& app, Document& document )
    : mWorld( World::Over( app, document.SceneOutsideHistory(), WorldOptions{ .mEditing = true } ) )
{
}

World& WorldOf( App& app, Document& document )
{
    if ( not document.TryView() )
        document.SetView( CreateScope<DocumentWorld>( app, document ) );
    // The editor gives documents no other view.
    return static_cast<DocumentWorld&>( *document.TryView() ).Get();
}
}
