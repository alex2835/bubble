#include "bubble/editor/document_world.hpp"

namespace bubble
{
DocumentWorld::DocumentWorld( App& app, Document& document )
    : mWorld( World::Over( app, document.SceneOutsideHistory(), WorldOptions{ .mEditing = true } ) )
{
}

void DocumentWorld::PlaceCamera( const RenderScene& scene )
{
    if ( mPlaced )
        return;
    mPlaced = true;
    if ( not scene.mCameras.empty() )
        mCamera.LookFrom( scene.mCameras.front().mWorld, 10.0f );
}

DocumentWorld& ViewOf( App& app, Document& document )
{
    if ( not document.TryView() )
        document.SetView( CreateScope<DocumentWorld>( app, document ) );
    // The editor gives a scene's document no other view.
    return static_cast<DocumentWorld&>( *document.TryView() );
}

World& WorldOf( App& app, Document& document )
{
    return ViewOf( app, document ).Get();
}
}
