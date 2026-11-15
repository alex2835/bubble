// The runner: a game on its own. The project and its levels come with
// their stages; for now a scene made here - boxes on the ground under the
// sun, a warm lamp - drawn by the renderer, the camera circling it as the
// actions say: stages 5 and 6 checked by hand. The keyboard, a gamepad and
// touches drive "move_x"; the mouse touches too, for a screen without touch.
#include "bubble/assets/model_primitives.hpp"
#include "bubble/core/log.hpp"
#include "bubble/input/input.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/types/thread.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"

using namespace bubble;

namespace
{
ActionMap Controls()
{
    // The left half of the window turns the camera one way or the other.
    const TouchArea leftQuarter{ .mMin = { 0.0f, 0.0f }, .mMax = { 0.25f, 1.0f } };
    const TouchArea secondQuarter{ .mMin = { 0.25f, 0.0f }, .mMax = { 0.5f, 1.0f } };
    ActionMap map;
    map.Bind( "move_x", Key::A, -1.0f )
        .Bind( "move_x", Key::D )
        .Bind( "move_x", Key::Left, -1.0f )
        .Bind( "move_x", Key::Right )
        .Bind( "move_x", GamepadAxis::LeftX )
        .Bind( "move_x", leftQuarter, -1.0f )
        .Bind( "move_x", secondQuarter );
    return map;
}

Text Make( string_view utf8 )
{
    return *Text::TryFrom( utf8 );
}

// What the importer would have made of a project's models, in memory: the
// runner has no project yet.
struct Models
{
    hmap<string, string> mImported;
    AssetCatalog mCatalog;

    AssetId Add( string_view path, const ModelAsset& model )
    {
        const AssetId assetId = AssetId::Generate();
        mCatalog.Add( assetId, *AssetPath::TryFrom( path ) );
        mImported[ImportedPath( assetId, cModelKind ).String()] = WriteModelAsset( model );
        return assetId;
    }

    AssetRegistry::ReadImported Read()
    {
        return AssetRegistry::AtOnce( [this]( const AssetId& assetId, string_view kind ) {
            const auto found = mImported.find( ImportedPath( assetId, kind ).View() );
            if ( found == mImported.end() )
                return expected<string, string>( unexpected( "no such file"s ) );
            return expected<string, string>( found->second );
        } );
    }
};

Scene Level( Models& models )
{
    Scene scene;
    const Entity ground = CreateChild( scene, scene.Root(), Make( "ground" ) );
    scene.Add<Transform>( ground );
    scene.Add<Model>( ground, Model{ .mAsset = models.Add( "models/ground.glb", MakePlane( 20.0f ) ) } );

    const AssetId box = models.Add( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    const array<Transform, 5> boxes{
        Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } },
        Transform{ .mPosition = { 2.5f, 1.0f, -1.0f }, .mRotation = { 0.0f, 0.6f, 0.0f }, .mScale = vec3( 2.0f ) },
        Transform{ .mPosition = { -2.0f, 0.35f, 1.5f }, .mRotation = { 0.0f, 0.3f, 0.0f }, .mScale = vec3( 0.7f ) },
        Transform{ .mPosition = { -1.5f, 1.5f, -2.5f }, .mRotation = { 0.5f, 0.8f, 0.2f }, .mScale = vec3( 1.0f ) },
        Transform{ .mPosition = { 1.0f, 0.25f, 2.5f }, .mScale = { 3.0f, 0.5f, 0.5f } },
    };
    for ( const Transform& placed : boxes )
    {
        const Entity entity = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( entity, placed );
        scene.Add<Model>( entity, Model{ .mAsset = box } );
    }

    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -40.0f ), glm::radians( 35.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
    const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
    scene.Add<Transform>( lamp, Transform{ .mPosition = { -2.0f, 1.5f, 3.0f } } );
    scene.Add<PointLight>( lamp, PointLight{ .mColor = { .mR = 1.0f, .mG = 0.6f, .mB = 0.3f, .mA = 1.0f },
                                             .mIntensity = 15.0f,
                                             .mRange = 8.0f } );
    scene.Add<Camera>( CreateChild( scene, scene.Root(), Make( "camera" ) ) );
    return scene;
}

// Around the middle of the scene, `turn` radians round, looking down at it.
Transform Orbit( f32 turn )
{
    constexpr f32 cDistance = 11.0f;
    constexpr f32 cPitch = -0.55f;
    const vec3 position( sin( turn ) * cos( cPitch ) * cDistance, -sin( cPitch ) * cDistance,
                         cos( turn ) * cos( cPitch ) * cDistance );
    return Transform{ .mPosition = position, .mRotation = { cPitch, turn, 0.0f } };
}
}

int main()
{
    const auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = {}, .mMouseAsTouch = true } );
    if ( not platformMaybe )
    {
        LogError( "{}", platformMaybe.error() );
        return 1;
    }
    Platform& platform = **platformMaybe;
    const auto windowMaybe =
        platform.TryOpenWindow( WindowOptions{ .mTitle = "bubble runner", .mSize = { 960, 540 }, .mResizable = true } );
    if ( not windowMaybe )
    {
        LogError( "{}", windowMaybe.error() );
        return 1;
    }
    const Window& window = *windowMaybe;

    // What Jobs may still read as it goes outlives App.
    Models models;
    App app;
    AssetRegistry assets( models.mCatalog, models.Read(), app.GetJobs() );
    RegisterModelLoader( assets );
    app.AddModule( TransformModule() );
    app.AddModule( RenderModule( assets ) );
    World world( app, Level( models ) );
    const Entity camera = *TryResolve( world.GetScene(), EntityPath{ Make( "/camera" ) } );

    // The GPU after the window, and gone before it.
    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
    Scope<Renderer> renderer;
    Input input( Controls() );
    f32 turn = 2.2f;
    platform.Run( [&]( f64 elapsed ) {
        for ( const InputEvent& event : platform.Poll() )
            input.Handle( event );
        const Actions actions = input.LatchFrame();
        turn += actions.Axis( "move_x"_n ) * static_cast<f32>( elapsed ) * 1.5f;
        world.GetScene().Get<Transform>( camera ) = Orbit( turn );
        world.Frame( elapsed );
        app.GetJobs().RunMainQueue();

        if ( not gpu )
        {
            auto finishedMaybe = startup->TryFinish();
            if ( not finishedMaybe )
                return true;
            if ( not *finishedMaybe )
            {
                LogError( "{}", finishedMaybe->error() );
                return false;
            }
            gpu = std::move( **finishedMaybe );
            renderer = CreateScope<Renderer>( *gpu, assets );
        }
        gpu->Poll();
        GpuSurface& surface = *gpu->TrySurface();
        surface.Resize( window.PixelSize() );
        // Presenting waits for the display, which paces the loop.
        if ( auto viewMaybe = surface.TryAcquire() )
        {
            const RenderScene& scene = world.Get<RenderScene>();
            renderer->Draw(
                scene, scene.mCameras.front(),
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() } );
            surface.Present();
        }
#ifndef __EMSCRIPTEN__
        // Minimized: nothing to present, nothing to wait for.
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
#endif
        return not platform.QuitRequested();
    } );
    return 0;
}
