#pragma once
#include "bubble/assets/asset_registry.hpp"
#include "bubble/editor/editor_camera.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/types/handle.hpp"
#include "bubble/world/world.hpp"

namespace bubble
{
class Prefab;

using ThumbnailHandle = Handle<struct ThumbnailTag>;

// The pictures of the project's assets, for the editor to show them by - a
// model seen from three quarters, a material on a ball, a prefab put
// together -: asked for by
// asset, drawn a few a frame in a world of its own, kept as textures, and in
// the project's .bubble/thumbnails so the next time they come at once. What
// it has not drawn yet, or cannot draw, it has nothing for: the one asking
// shows something else - its kind's icon. One a project, beside the editor;
// nothing else of the editor goes through it.
class ThumbnailService
{
public:
    // Its pictures kept among the project's files, in .bubble/thumbnails.
    ThumbnailService( EditorProject& project, Gpu& gpu, Renderer& renderer );
    ~ThumbnailService();
    ThumbnailService( const ThumbnailService& ) = delete;
    ThumbnailService& operator=( const ThumbnailService& ) = delete;

    // Whether it draws assets of `kind`: models, materials, prefabs.
    static bool Draws( string_view kind );

    // The asset's picture asked for: the same asset, the same handle. Drawn
    // later, in Pump; an asset of a kind it does not draw has a handle that
    // never has one.
    ThumbnailHandle Request( const AssetId& assetId, string_view kind );
    // Its picture: nothing while it is not drawn yet - or cannot be -, or
    // for a handle it does not know. One drawn again after its asset
    // changed shows the last until the new one is there.
    OptRef<const GpuTexture> TryGet( ThumbnailHandle handle ) const;

    // Once a frame, after ImGui's lists are made and before they are drawn:
    // a few asked for are read from the cache or drawn.
    void Pump();
    // Asked for and not drawn yet, and how many it drew - not read from the
    // cache - since it was made: for tests.
    size_t Waiting() const;
    u64 Drawn() const { return mDrawn; }

    // A picture's side, in pixels.
    static constexpr u32 cSize = 160;
    // Pictures drawn - not read from the cache - in one Pump.
    static constexpr u32 cDrawsAFrame = 2;

private:
    enum class State : u8
    {
        Waiting,
        Ready,
        // Drawn never: its asset failed, or the kind is not drawn.
        None,
    };
    struct Thumb
    {
        AssetId mAsset;
        string mKind;
        State mState = State::Waiting;
        GpuTexture mPicture;
        // A picture there, Ready or drawn before it changed.
        bool mShown = false;
    };
    // An asset being made ready to draw: what it needs, loading.
    struct Job;

    void Start( ThumbnailHandle handle );
    // True when the job is done, drawn or given up.
    bool Step( Job& job, u32& draws );
    // An asset the job draws with, loaded by its kind - once.
    void Need( Job& job, const AssetId& assetId );
    // Everything the job needs is in - or failed -: a material's bases,
    // shader and textures asked for as it comes.
    bool Loaded( Job& job );
    // The cache's key for all the job asked for.
    string KeyOfAll( const Job& job ) const;
    void Draw( Thumb& thumb, const AssetId& model, vector<AssetId> materials, vec3 low, vec3 high );
    void DrawPrefab( Thumb& thumb, const Prefab& prefab );
    // The studio as it stands, framed on `low` to `high`, into its picture.
    void Photograph( Thumb& thumb, vec3 low, vec3 high );
    // The cache's key for the asset: what its import - and its material's
    // bases, shader and textures - were made from.
    string KeyOf( const AssetId& assetId ) const;
    bool TryReadCache( Thumb& thumb, const string& key );
    void WriteCache( const Thumb& thumb, const string& key );
    AssetPath CachePath( const AssetId& assetId ) const;
    void Changed( const AssetEntryBase& entry );
    GpuTexture& PictureOf( Thumb& thumb );

    EditorProject& mProject;
    Gpu& mGpu;
    Renderer& mRenderer;
    SlotMap<Thumb, ThumbnailTag> mThumbs;
    hmap<AssetId, ThumbnailHandle> mByAsset;
    vector<ThumbnailHandle> mQueue;
    vector<Scope<Job>> mJobs;
    AssetListenerHandle mListening;
    // The world they are drawn in: one subject, two lights.
    Scope<World> mStudio;
    Entity mSubject;
    RenderView mView;
    AssetId mBallId = AssetId::Generate();
    AssetRef<ModelAsset> mBall;
    u64 mDrawn = 0;
};
}
