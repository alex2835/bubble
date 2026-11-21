#include "bubble/editor/graph_preview.hpp"
#include "bubble/animation/animation.hpp"
#include "bubble/animation/animation_assets.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"

namespace bubble
{
GraphPreview::GraphPreview( EditorProject& project, const DataDocument& document ) : mProject( project )
{
    // In the registry before the world loads it: the rest pose until the
    // document's graph reads.
    mGraph = mProject.GetAssets().Set<AnimationGraph>( mGraphId, AnimationGraph() );
    Scene scene;
    mSubject = CreateChild( scene, scene.Root(), *Text::TryFrom( "subject" ) );
    scene.Add<Transform>( mSubject );
    const Entity sun = CreateChild( scene, scene.Root(), *Text::TryFrom( "sun" ) );
    scene.Add<Transform>(
        sun, Transform{ .mPosition = vec3( 0.0f ), .mRotation = vec3( -0.7f, 0.6f, 0.0f ), .mScale = vec3( 1.0f ) } );
    scene.Add<DirectionalLight>( sun, DirectionalLight{ .mColor = Color(), .mIntensity = 3.0f, .mShadows = true } );
    // Every module: an animator steps only in a world that steps.
    mWorld = CreateScope<World>( project.GetApp(), std::move( scene ), WorldOptions{ .mEditing = false } );
    mCamera.mPitch = -0.2f;
    PickModel( document );
}

vector<AssetPath> GraphPreview::Models() const
{
    vector<AssetPath> models;
    for ( const AssetPath& path : mProject.GetCatalog().Paths() )
        if ( mProject.Makes( path, cClipsKind ) )
            models.push_back( path );
    ranges::sort( models, []( const AssetPath& a, const AssetPath& b ) { return a.View() < b.View(); } );
    return models;
}

OptRef<const AssetPath> GraphPreview::TryModel() const
{
    return mModelMaybe ? OptRef<const AssetPath>( *mModelMaybe ) : nullopt;
}

void GraphPreview::PickModel( const DataDocument& document )
{
    // As the scene shown has it: the model, and its materials with it.
    if ( const auto graphIdMaybe = mProject.GetCatalog().TryIdOf( document.GetPath() ) )
        if ( const auto shownMaybe = mProject.GetWorkspace().TryShownScene() )
        {
            const Scene& scene = shownMaybe->GetScene();
            for ( const Entity entity : scene.EntitiesWith<Animator>() )
            {
                const auto modelMaybe = scene.TryGet<Model>( entity );
                if ( scene.Get<Animator>( entity ).mGraph != *graphIdMaybe or not modelMaybe )
                    continue;
                if ( const auto pathMaybe = mProject.GetCatalog().TryPathOf( modelMaybe->mAsset ) )
                {
                    mModelMaybe = *pathMaybe;
                    Scene& own = mWorld->GetScene();
                    own.Add<Model>( mSubject, *modelMaybe );
                    own.Add<Animator>( mSubject, Animator{ .mGraph = mGraphId, .mSpeed = 1 } );
                    return;
                }
            }
        }
    if ( const vector<AssetPath> models = Models(); not models.empty() )
        SetModel( models.front() );
}

void GraphPreview::SetModel( const AssetPath& path )
{
    const auto idMaybe = mProject.GetCatalog().TryIdOf( path );
    if ( not idMaybe )
        return;
    mModelMaybe = path;
    mFramed = false;
    Scene& scene = mWorld->GetScene();
    if ( scene.Has<Model>( mSubject ) )
        scene.Remove<Model>( mSubject );
    scene.Add<Model>( mSubject, Model{ .mAsset = *idMaybe, .mMaterials = {}, .mTint = Color() } );
    if ( not scene.Has<Animator>( mSubject ) )
        scene.Add<Animator>( mSubject, Animator{ .mGraph = mGraphId, .mSpeed = 1 } );
}

f32 GraphPreview::Parameter( const string& name ) const
{
    const auto found = mParameters.find( name );
    return found != mParameters.end() ? found->second : 0.0f;
}

void GraphPreview::SetParameter( const string& name, f32 value )
{
    mParameters[name] = value;
}

void GraphPreview::Trigger( const string& name )
{
    mTriggers.push_back( name );
}

opt<string> GraphPreview::TryState() const
{
    return const_cast<World&>( *mWorld ).Get<AnimationWorld>().TryState( mSubject );
}

void GraphPreview::Frame( const DataDocument& document, f64 elapsed )
{
    if ( document.Version() != mShownVersion )
    {
        mShownVersion = document.Version();
        auto graphMaybe = TryParseAnimationGraph( document.GetData().Write() );
        if ( graphMaybe )
        {
            mError.clear();
            // Parameters it starts with, as the graph has them, where none
            // was set here.
            for ( size_t at = 0; at < graphMaybe->mParameters.size(); ++at )
                mParameters.try_emplace( graphMaybe->mParameters[at].mName, graphMaybe->mParameters[at].mDefault );
            mGraph = mProject.GetAssets().Set<AnimationGraph>( mGraphId, std::move( *graphMaybe ) );
        }
        else
            mError = format( "{}:{}", document.GetPath().View(), graphMaybe.error() );
    }
    // The parameters as set here, before the steps the frame takes.
    AnimationWorld& animation = mWorld->Get<AnimationWorld>();
    if ( const auto graphMaybe = mGraph.TryGet() )
        for ( const AnimationParameter& parameter : graphMaybe->mParameters )
        {
            if ( parameter.mKind == AnimationParameterKind::Number )
                (void)animation.TrySet( mSubject, parameter.mName, Parameter( parameter.mName ) );
            else if ( parameter.mKind == AnimationParameterKind::Bool )
                (void)animation.TrySet( mSubject, parameter.mName, Parameter( parameter.mName ) != 0.0f );
        }
    for ( const string& trigger : mTriggers )
        (void)animation.TryTrigger( mSubject, trigger );
    mTriggers.clear();
    mWorld->Frame( elapsed );

    // Framed on the model once it has come.
    if ( mFramed )
        return;
    for ( const RenderItem& item : mWorld->Get<RenderScene>().mItems )
        if ( const auto modelMaybe = item.mModel.TryGet() )
        {
            const auto [low, high] = BoundsOf( item.mWorld, modelMaybe->mMin, modelMaybe->mMax );
            mCamera.Frame( low, high );
            mFramed = true;
        }
}
}
