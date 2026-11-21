#include "bubble/editor/document_world.hpp"
#include "bubble/assets/model_asset.hpp"

namespace bubble
{
namespace
{
// How long the camera waits for models that are loading: one that never
// comes does not keep it where it is.
constexpr u32 cFramesWaited = 120;
}

RenderLight EditorSun()
{
    return RenderLight{ .mKind = RenderLightKind::Directional,
                        .mPosition = vec3( 0.0f ),
                        .mDirection = glm::normalize( vec3( 0.4f, -1.0f, -0.6f ) ),
                        .mRadiance = vec3( 3.0f ),
                        .mRange = 0,
                        .mInnerCos = -1,
                        .mOuterCos = -1,
                        .mShadows = true };
}

DocumentWorld::DocumentWorld( App& app, Document& document, WorldOptions options )
    : mDocument( document ),
      mWorld( World::Over( app, document.SceneOutsideHistory(), options ) )
{
}

void DocumentWorld::Gathered()
{
    RenderScene& scene = mWorld->Get<RenderScene>();
    // Gathered again every frame, so the sun is there only as long as the
    // prefab has no light.
    if ( mDocument.TryPrefabNodes() and scene.mLights.empty() )
        scene.mLights.push_back( EditorSun() );
    if ( mPlaced )
        return;
    // A prefab's camera is the game's - a player's, following it -: its
    // view frames the prefab itself.
    if ( not scene.mCameras.empty() and not mDocument.TryPrefabNodes() )
    {
        mPlaced = true;
        mCamera.LookFrom( scene.mCameras.front().mWorld, 10.0f );
        return;
    }
    vec3 low( numeric_limits<f32>::max() );
    vec3 high( -numeric_limits<f32>::max() );
    bool loading = false;
    for ( const RenderItem& item : scene.mItems )
    {
        if ( item.mModel.State() == AssetState::Loading )
            loading = true;
        else if ( const auto modelMaybe = item.mModel.TryGet() )
        {
            const auto [itemLow, itemHigh] = BoundsOf( item.mWorld, modelMaybe->mMin, modelMaybe->mMax );
            low = glm::min( low, itemLow );
            high = glm::max( high, itemHigh );
        }
    }
    if ( loading and ++mWaited < cFramesWaited )
        return;
    mPlaced = true;
    if ( low.x <= high.x )
        mCamera.Frame( low, high );
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
