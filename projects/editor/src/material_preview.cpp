#include "bubble/editor/material_preview.hpp"
#include "bubble/assets/model_primitives.hpp"
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"

namespace bubble
{
namespace
{
// The engine's own material, for one with no shader: as GpuMaterials names it.
constexpr string_view cStandardShader = "standard_material.wgsl";

string StampOf( const AssetEntryBase& entry )
{
    return format( "{}:{}:{};", entry.Id().ToString(), entry.Version(), static_cast<int>( entry.State() ) );
}
}

MaterialPreview::MaterialPreview( EditorProject& project, const DataDocument& document ) : mProject( project )
{
    // In the registry before the world loads them.
    mSphere = mProject.GetAssets().Set<ModelAsset>( mSphereId, MakeSphere( 0.5f, 24 ) );
    Refresh( document );

    Scene scene;
    const Entity subject = CreateChild( scene, scene.Root(), *Text::TryFrom( "subject" ) );
    scene.Add<Transform>( subject );
    scene.Add<Model>( subject, Model{ .mAsset = mSphereId, .mMaterials = { mMaterialId }, .mTint = Color() } );
    const Entity sun = CreateChild( scene, scene.Root(), *Text::TryFrom( "sun" ) );
    scene.Add<Transform>(
        sun, Transform{ .mPosition = vec3( 0.0f ), .mRotation = vec3( -0.7f, 0.6f, 0.0f ), .mScale = vec3( 1.0f ) } );
    scene.Add<DirectionalLight>( sun, DirectionalLight{ .mColor = Color(), .mIntensity = 3.0f, .mShadows = false } );
    mWorld = CreateScope<World>( project.GetApp(), std::move( scene ), WorldOptions{ .mEditing = true } );
    mCamera.mDistance = 1.8f;
    mCamera.mPitch = -0.3f;
}

void MaterialPreview::Refresh( const DataDocument& document )
{
    if ( document.Version() != mShownVersion )
    {
        mShownVersion = document.Version();
        auto materialMaybe = TryReadMaterialAsset( document.GetData().Write() );
        if ( materialMaybe )
        {
            mReadError.clear();
            mReadMaybe = *materialMaybe;
            mMaterial = mProject.GetAssets().Set<MaterialAsset>( mMaterialId, std::move( *materialMaybe ) );
        }
        else
        {
            mReadError = format( "{}: {}", document.GetPath().View(), materialMaybe.error() );
            mReadMaybe.reset();
            // The standard material until it reads.
            if ( not mMaterial )
                mMaterial = mProject.GetAssets().Set<MaterialAsset>( mMaterialId, MaterialAsset() );
        }
    }
    Resolve( document );
}

void MaterialPreview::Resolve( const DataDocument& document )
{
    if ( not mReadMaybe )
    {
        mLayoutMaybe.reset();
        mBases.clear();
        mError = mReadError;
        return;
    }
    AssetRegistry& assets = mProject.GetAssets();
    string stamp = format( "{};", mShownVersion );
    vector<AssetRef<MaterialAsset>> baseRefs;
    vector<MaterialAsset> bases;
    string error;
    bool waiting = false;
    // A base that leads back to the document - or round any other way - is
    // as GpuMaterials would find it.
    vector<AssetId> seen;
    if ( const auto ownIdMaybe = mProject.GetCatalog().TryIdOf( document.GetPath() ) )
        seen.push_back( *ownIdMaybe );
    AssetId shaderId = mReadMaybe->mShader;
    for ( AssetId base = mReadMaybe->mBase; base; )
    {
        const AssetRef<MaterialAsset> baseRef = assets.Load<MaterialAsset>( base );
        baseRefs.push_back( baseRef );
        stamp += StampOf( baseRef.Entry() );
        // Compared one by one: the standard library's vectorized find takes
        // no 16-byte elements.
        if ( ranges::any_of( seen, [&]( const AssetId& seenId ) { return seenId == base; } ) )
        {
            error = format( "{}: its bases go round, back to {}", document.GetPath().View(),
                            baseRef.Entry().Path().View() );
            break;
        }
        seen.push_back( base );
        if ( baseRef.State() == AssetState::Failed )
        {
            error = format( "{}: its base {}", document.GetPath().View(), baseRef.Entry().Error() );
            break;
        }
        if ( not baseRef.Ready() )
        {
            waiting = true;
            break;
        }
        bases.push_back( *baseRef.TryGet() );
        shaderId = bases.back().mShader;
        base = bases.back().mBase;
    }
    AssetRef<ShaderAsset> shader;
    if ( error.empty() and not waiting and shaderId )
    {
        shader = assets.Load<ShaderAsset>( shaderId );
        stamp += StampOf( shader.Entry() );
        if ( shader.State() == AssetState::Failed )
            error = format( "{}: its shader {}", document.GetPath().View(), shader.Entry().Error() );
        else if ( not shader.Ready() )
            waiting = true;
    }
    if ( stamp == mResolved )
        return;
    mResolved = std::move( stamp );
    mBaseRefs = std::move( baseRefs );
    mBases = std::move( bases );
    mShader = std::move( shader );
    mLayoutMaybe.reset();
    mError = std::move( error );
    if ( not mError.empty() or waiting )
        return;
    auto layoutMaybe = mShader ? TryReflectMaterial( mShader.TryGet()->mSource, mShader.Entry().Path().View() )
                               : TryReflectMaterial( EngineShader( cStandardShader ), cStandardShader );
    if ( layoutMaybe )
        mLayoutMaybe = std::move( *layoutMaybe );
    else
        mError = std::move( layoutMaybe.error() );
}

OptRef<const MaterialAsset> MaterialPreview::TryMaterial() const
{
    return mReadMaybe ? OptRef<const MaterialAsset>( *mReadMaybe ) : OptRef<const MaterialAsset>();
}

OptRef<const MaterialLayout> MaterialPreview::TryLayout() const
{
    return mLayoutMaybe ? OptRef<const MaterialLayout>( *mLayoutMaybe ) : OptRef<const MaterialLayout>();
}

MaterialPreview& PreviewOf( EditorProject& project, DataDocument& document )
{
    if ( not document.TryView() )
        document.SetView( CreateScope<MaterialPreview>( project, document ) );
    // The editor gives a material's document no other view.
    return static_cast<MaterialPreview&>( *document.TryView() );
}
}
