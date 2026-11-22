#include "bubble/editor/thumbnail_service.hpp"
#include "bubble/assets/material_asset.hpp"
#include "bubble/assets/model_primitives.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/core/log.hpp"
#include "bubble/data/binary.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/render_scene.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scene/prefab.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/world/transform.hpp"

namespace bubble
{
namespace
{
// A cached picture's file: "BTHM", its version, the key it was drawn for,
// its size, its RGBA8 pixels.
constexpr u32 cMagic = 0x4D485442;
constexpr u32 cVersion = 1;
// Jobs loading at once.
constexpr size_t cJobs = 4;
// A material's bases followed at most this far: past it they go round.
constexpr size_t cMostBases = 16;
// Behind a picture: the tiles' grey, a little lighter.
constexpr Color cBackdrop{ .mR = 0.2f, .mG = 0.2f, .mB = 0.21f, .mA = 1.0f };

Entity Light( Scene& scene, string_view name, vec3 angles, f32 intensity )
{
    const Entity light = CreateChild( scene, scene.Root(), *Text::TryFrom( name ) );
    scene.Add<Transform>( light, Transform{ .mPosition = vec3( 0.0f ), .mRotation = angles, .mScale = vec3( 1.0f ) } );
    scene.Add<DirectionalLight>( light,
                                 DirectionalLight{ .mColor = Color(), .mIntensity = intensity, .mShadows = false } );
    return light;
}

// Each asset `value` names, however deep in it.
template <typename F>
void ForEachAsset( const Value& value, F&& fn )
{
    if ( const auto assetMaybe = value.TryAs<AssetId>() )
        fn( *assetMaybe );
    else if ( const auto listMaybe = value.TryAs<Value::List>() )
        for ( const Value& item : *listMaybe )
            ForEachAsset( item, fn );
    else if ( const auto recordMaybe = value.TryAs<Value::Record>() )
        for ( const ValueField& field : *recordMaybe )
            ForEachAsset( field.mValue, fn );
}
}

struct ThumbnailService::Job
{
    ThumbnailHandle mHandle;
    bool mStarted = false;
    AssetRef<PrefabAsset> mPrefabAsset;
    opt<Prefab> mPrefab;
    // What it is drawn with, loading: models, materials - their bases
    // among them -, shaders, textures; each asked for once.
    vector<AssetRef<ModelAsset>> mModels;
    vector<AssetRef<MaterialAsset>> mMaterials;
    vector<AssetRef<ShaderAsset>> mShaders;
    vector<AssetRef<TextureAsset>> mTextures;
    hset<AssetId> mAsked;
    // The materials looked into so far, for their bases, shaders, textures.
    size_t mFollowed = 0;
};

ThumbnailService::ThumbnailService( EditorProject& project, Gpu& gpu, Renderer& renderer )
    : mProject( project ),
      mGpu( gpu ),
      mRenderer( renderer )
{
    // In the registry before the studio loads it.
    mBall = mProject.GetAssets().Set<ModelAsset>( mBallId, MakeSphere( 0.5f, 32 ) );
    Scene scene;
    mSubject = CreateChild( scene, scene.Root(), *Text::TryFrom( "subject" ) );
    scene.Add<Transform>( mSubject );
    Light( scene, "key", vec3( -0.7f, 0.7f, 0.0f ), 3.0f );
    Light( scene, "fill", vec3( -0.3f, -2.4f, 0.0f ), 0.8f );
    mStudio = CreateScope<World>( project.GetApp(), std::move( scene ), WorldOptions{ .mEditing = true } );
    mView.SetBackground( cBackdrop );
    mListening = mProject.GetAssets().OnChanged( [this]( const AssetEntryBase& entry ) { Changed( entry ); } );
}

ThumbnailService::~ThumbnailService()
{
    mProject.GetAssets().RemoveListener( mListening );
}

bool ThumbnailService::Draws( string_view kind )
{
    return kind == cModelKind or kind == cMaterialKind or kind == cPrefabKind;
}

ThumbnailHandle ThumbnailService::Request( const AssetId& assetId, string_view kind )
{
    if ( const auto found = mByAsset.find( assetId ); found != mByAsset.end() )
        return found->second;
    const ThumbnailHandle handle = mThumbs.Add( Thumb{ .mAsset = assetId,
                                                       .mKind = string( kind ),
                                                       .mState = Draws( kind ) ? State::Waiting : State::None,
                                                       .mPicture = GpuTexture(),
                                                       .mShown = false } );
    mByAsset[assetId] = handle;
    if ( Draws( kind ) )
        mQueue.push_back( handle );
    return handle;
}

OptRef<const GpuTexture> ThumbnailService::TryGet( ThumbnailHandle handle ) const
{
    const auto thumbMaybe = mThumbs.TryGet( handle );
    if ( not thumbMaybe or not thumbMaybe->mShown )
        return nullopt;
    return thumbMaybe->mPicture;
}

size_t ThumbnailService::Waiting() const
{
    size_t waiting = 0;
    for ( const ThumbnailHandle handle : mThumbs.Handles() )
        if ( mThumbs.TryGet( handle )->mState == State::Waiting )
            ++waiting;
    return waiting;
}

void ThumbnailService::Pump()
{
    u32 draws = 0;
    // A job read from the cache is done at once and makes room for the next.
    for ( size_t started = 0; started < cJobs * 2; ++started )
    {
        while ( mJobs.size() < cJobs and not mQueue.empty() )
        {
            const ThumbnailHandle handle = mQueue.front();
            mQueue.erase( mQueue.begin() );
            Start( handle );
        }
        const size_t before = mJobs.size();
        erase_if( mJobs, [&]( const Scope<Job>& job ) { return Step( *job, draws ); } );
        if ( mJobs.size() == before or mQueue.empty() )
            break;
    }
}

void ThumbnailService::Start( ThumbnailHandle handle )
{
    if ( not mThumbs.Alive( handle ) )
        return;
    auto job = CreateScope<Job>();
    job->mHandle = handle;
    mJobs.push_back( std::move( job ) );
}

void ThumbnailService::Need( Job& job, const AssetId& assetId )
{
    if ( not assetId or not job.mAsked.insert( assetId ).second )
        return;
    const auto pathMaybe = mProject.GetCatalog().TryPathOf( assetId );
    if ( not pathMaybe )
        return;
    AssetRegistry& assets = mProject.GetAssets();
    const string_view kind = mProject.KindOf( *pathMaybe );
    if ( kind == cModelKind )
        job.mModels.push_back( assets.Load<ModelAsset>( assetId ) );
    else if ( kind == cMaterialKind )
        job.mMaterials.push_back( assets.Load<MaterialAsset>( assetId ) );
    else if ( kind == cShaderKind )
        job.mShaders.push_back( assets.Load<ShaderAsset>( assetId ) );
    else if ( kind == cTextureKind )
        job.mTextures.push_back( assets.Load<TextureAsset>( assetId ) );
}

bool ThumbnailService::Loaded( Job& job )
{
    // Each material as it comes: what it is made of asked for in turn.
    for ( ; job.mFollowed < job.mMaterials.size(); ++job.mFollowed )
    {
        const AssetRef<MaterialAsset>& material = job.mMaterials[job.mFollowed];
        if ( material.State() == AssetState::Loading )
            return false;
        if ( not material.Ready() or job.mMaterials.size() >= cMostBases * 4 )
            continue;
        const MaterialAsset& made = *material.TryGet();
        Need( job, made.mBase );
        Need( job, made.mShader );
        for ( const auto& [name, value] : made.mParams )
            Need( job, value.mTexture );
    }
    // One that fails is drawn as the renderer draws it.
    const auto loading = []( const auto& ref ) { return ref.State() == AssetState::Loading; };
    return ranges::none_of( job.mModels, loading ) and ranges::none_of( job.mShaders, loading ) and
           ranges::none_of( job.mTextures, loading );
}

string ThumbnailService::KeyOfAll( const Job& job ) const
{
    vector<AssetId> ids( job.mAsked.begin(), job.mAsked.end() );
    ranges::sort( ids );
    string key;
    for ( const AssetId& assetId : ids )
        key += KeyOf( assetId );
    return key;
}

bool ThumbnailService::Step( Job& job, u32& draws )
{
    const auto thumbMaybe = mThumbs.TryGet( job.mHandle );
    if ( not thumbMaybe )
        return true;
    Thumb& thumb = *thumbMaybe;
    AssetRegistry& assets = mProject.GetAssets();
    const auto giveUp = [&] {
        thumb.mState = State::None;
        return true;
    };
    const auto drawn = [&]( const string& key ) {
        ++draws;
        WriteCache( thumb, key );
        thumb.mState = State::Ready;
        return true;
    };

    if ( thumb.mKind == cModelKind )
    {
        if ( not job.mStarted )
        {
            job.mStarted = true;
            if ( TryReadCache( thumb, KeyOf( thumb.mAsset ) ) )
                return true;
            Need( job, thumb.mAsset );
        }
        if ( job.mModels.empty() or job.mModels.front().State() == AssetState::Failed )
            return giveUp();
        if ( not job.mModels.front().Ready() or draws >= cDrawsAFrame )
            return false;
        const ModelAsset& model = *job.mModels.front().TryGet();
        Draw( thumb, thumb.mAsset, {}, model.mMin, model.mMax );
        return drawn( KeyOf( thumb.mAsset ) );
    }

    if ( thumb.mKind == cMaterialKind )
    {
        if ( not job.mStarted )
        {
            job.mStarted = true;
            Need( job, thumb.mAsset );
        }
        if ( job.mMaterials.empty() or job.mMaterials.front().State() == AssetState::Failed )
            return giveUp();
        if ( not Loaded( job ) )
            return false;
        const string key = KeyOfAll( job );
        if ( TryReadCache( thumb, key ) )
            return true;
        if ( draws >= cDrawsAFrame )
            return false;
        Draw( thumb, mBallId, { thumb.mAsset }, vec3( -0.5f ), vec3( 0.5f ) );
        return drawn( key );
    }

    // A prefab: read, then what its nodes name - models, materials - loaded.
    if ( not job.mStarted )
    {
        job.mStarted = true;
        job.mPrefabAsset = assets.Load<PrefabAsset>( thumb.mAsset );
    }
    if ( job.mPrefabAsset.State() == AssetState::Failed )
        return giveUp();
    if ( not job.mPrefabAsset.Ready() )
        return false;
    if ( not job.mPrefab )
    {
        auto prefabMaybe = Prefab::TryParse( job.mPrefabAsset.TryGet()->mText,
                                             FileContext( mProject.GetApp().GetComponentTypes(), assets.Catalog() ) );
        if ( not prefabMaybe )
            return giveUp();
        job.mPrefab = std::move( *prefabMaybe );
        // Its own key and those of what it names: read from the cache when
        // none of them changed.
        job.mAsked.insert( thumb.mAsset );
        for ( const Prefab::Node& node : job.mPrefab->Nodes() )
            for ( const auto& [component, value] : node.mComponents )
                ForEachAsset( value, [&]( const AssetId& assetId ) { Need( job, assetId ); } );
        if ( TryReadCache( thumb, KeyOfAll( job ) ) )
            return true;
    }
    if ( not Loaded( job ) or draws >= cDrawsAFrame )
        return false;
    DrawPrefab( thumb, *job.mPrefab );
    return drawn( KeyOfAll( job ) );
}

GpuTexture& ThumbnailService::PictureOf( Thumb& thumb )
{
    if ( not thumb.mPicture )
        thumb.mPicture = GpuTexture(
            mGpu, GpuTextureOptions{ .mSize = ivec2( static_cast<i32>( cSize ) ),
                                     .mMips = 1,
                                     .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                     .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding |
                                               WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
                                     .mLabel = "thumbnail" } );
    return thumb.mPicture;
}

void ThumbnailService::Draw( Thumb& thumb, const AssetId& model, vector<AssetId> materials, vec3 low, vec3 high )
{
    // The subject made again: the studio's modules hear it go and come.
    Scene& scene = mStudio->GetScene();
    if ( scene.Has<Model>( mSubject ) )
        scene.Remove<Model>( mSubject );
    scene.Add<Model>( mSubject, Model{ .mAsset = model, .mMaterials = std::move( materials ), .mTint = Color() } );
    mStudio->Frame( 0.0 );
    Photograph( thumb, low, high );
}

void ThumbnailService::DrawPrefab( Thumb& thumb, const Prefab& prefab )
{
    // Its instance in the studio, the subject's model out of the way, and
    // gone again once it is drawn.
    Scene& scene = mStudio->GetScene();
    if ( scene.Has<Model>( mSubject ) )
        scene.Remove<Model>( mSubject );
    const Entity instance = Instantiate( scene, prefab, thumb.mAsset, scene.Root(), *Text::TryFrom( "instance" ),
                                         mProject.GetApp().GetComponentTypes() );
    mStudio->Frame( 0.0 );
    // Everything it shows, as the frame placed it.
    vec3 low( numeric_limits<f32>::max() );
    vec3 high( numeric_limits<f32>::lowest() );
    for ( const RenderItem& item : mStudio->Get<RenderScene>().mItems )
    {
        const auto modelMaybe = item.mModel.TryGet();
        if ( not modelMaybe )
            continue;
        for ( int corner = 0; corner < 8; ++corner )
        {
            const vec3 at( corner & 1 ? modelMaybe->mMax.x : modelMaybe->mMin.x,
                           corner & 2 ? modelMaybe->mMax.y : modelMaybe->mMin.y,
                           corner & 4 ? modelMaybe->mMax.z : modelMaybe->mMin.z );
            const vec3 placed( item.mWorld * vec4( at, 1.0f ) );
            low = glm::min( low, placed );
            high = glm::max( high, placed );
        }
    }
    // Nothing to see: a unit's room about its origin.
    if ( low.x > high.x )
    {
        low = vec3( -0.5f );
        high = vec3( 0.5f );
    }
    Photograph( thumb, low, high );
    scene.Destroy( instance );
}

void ThumbnailService::Photograph( Thumb& thumb, vec3 low, vec3 high )
{
    // From the front, to the right and above, as far as the whole of it
    // fills the picture.
    EditorCamera camera;
    camera.mFov = 0.6f;
    camera.mFocus = ( low + high ) * 0.5f;
    const f32 radius = max( glm::length( high - low ) * 0.5f, 0.01f );
    camera.mDistance = radius / std::sin( camera.mFov * 0.5f ) * 1.05f;
    GpuTexture& picture = PictureOf( thumb );
    const RenderTarget target{ .mView = picture.View(), .mSize = picture.Size(), .mFormat = picture.Format() };
    // Twice: the first draw makes the GPU's copies of what it uses.
    for ( int draw = 0; draw < 2; ++draw )
        mRenderer.Draw( mView, mStudio->Get<RenderScene>(), camera.ToRenderCamera(), target );
    thumb.mShown = true;
    ++mDrawn;
}

string ThumbnailService::KeyOf( const AssetId& assetId ) const
{
    const auto keyMaybe =
        mProject.GetFiles().TryRead( *AssetPath::TryFrom( format( ".bubble/imported/{}.key", assetId.ToString() ) ) );
    // No key - an asset made in memory - is drawn every time.
    return format( "{}={};", assetId.ToString(), keyMaybe ? *keyMaybe : "-"s );
}

AssetPath ThumbnailService::CachePath( const AssetId& assetId ) const
{
    return *AssetPath::TryFrom( format( ".bubble/thumbnails/{}.thumb", assetId.ToString() ) );
}

bool ThumbnailService::TryReadCache( Thumb& thumb, const string& key )
{
    const auto bytesMaybe = mProject.GetFiles().TryRead( CachePath( thumb.mAsset ) );
    if ( not bytesMaybe )
        return false;
    BinaryReader reader( *bytesMaybe );
    const auto magicMaybe = reader.TryU32();
    const auto versionMaybe = reader.TryU32();
    const auto keyMaybe = reader.TryText();
    const auto widthMaybe = reader.TryU32();
    const auto heightMaybe = reader.TryU32();
    if ( magicMaybe != cMagic or versionMaybe != cVersion or not keyMaybe or *keyMaybe != key or widthMaybe != cSize or
         heightMaybe != cSize )
        return false;
    const auto pixelsMaybe = reader.TryBytes( u64{ cSize } * cSize * 4 );
    if ( not pixelsMaybe )
        return false;
    PictureOf( thumb ).Write( mGpu, 0, as_bytes( span<const char>( pixelsMaybe->data(), pixelsMaybe->size() ) ) );
    thumb.mShown = true;
    thumb.mState = State::Ready;
    return true;
}

void ThumbnailService::WriteCache( const Thumb& thumb, const string& key )
{
    auto pixelsMaybe = mGpu.TryReadRgba8( thumb.mPicture );
    if ( not pixelsMaybe )
    {
        LogError( "thumbnail: {}", pixelsMaybe.error() );
        return;
    }
    BinaryWriter writer;
    writer.U32( cMagic );
    writer.U32( cVersion );
    writer.Text( key );
    writer.U32( cSize );
    writer.U32( cSize );
    writer.Bytes( span<const u8>( *pixelsMaybe ) );
    if ( auto writtenMaybe = mProject.GetFiles().TryWrite( CachePath( thumb.mAsset ), writer.Take() );
         not writtenMaybe )
        LogError( "thumbnail: {}", writtenMaybe.error() );
}

void ThumbnailService::Changed( const AssetEntryBase& entry )
{
    // A material's picture follows its bases, shader and textures too.
    const bool feedsMaterials = entry.Type() == type_index( typeid( MaterialAsset ) ) or
                                entry.Type() == type_index( typeid( ShaderAsset ) ) or
                                entry.Type() == type_index( typeid( TextureAsset ) );
    for ( const ThumbnailHandle handle : mThumbs.Handles() )
    {
        Thumb& thumb = *mThumbs.TryGet( handle );
        // A prefab's picture follows whatever its nodes name.
        const bool mine = thumb.mAsset == entry.Id() or ( feedsMaterials and thumb.mKind == cMaterialKind ) or
                          thumb.mKind == cPrefabKind;
        if ( not mine or not Draws( thumb.mKind ) or thumb.mState == State::Waiting )
            continue;
        // Drawn again; the last picture shown until then.
        thumb.mState = State::Waiting;
        mQueue.push_back( handle );
    }
}
}
