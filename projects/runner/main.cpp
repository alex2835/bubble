// The runner: a game on its own. The project and its levels come with
// their stages; for now a scene made here - boxes of a few materials on the
// ground under the sun, a warm lamp - drawn by the renderer, the camera
// circling it as the actions say: stages 5 and 6 checked by hand. The
// keyboard, a gamepad and touches drive "move_x"; the mouse touches too, for
// a screen without touch. A touch or a click picks what it lands on: its box
// drawn around it, its path logged. `--frames N` quits after N frames
// drawn: a run a script can check.
#include "bubble/assets/model_primitives.hpp"
#include "bubble/assets/shader_asset.hpp"
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

// What the importer would have made of a project's assets, in memory: the
// runner has no project yet.
struct Project
{
    hmap<string, string> mImported;
    AssetCatalog mCatalog;

    AssetId Add( string_view path, string bytes, string_view kind )
    {
        const AssetId assetId = AssetId::Generate();
        mCatalog.Add( assetId, *AssetPath::TryFrom( path ) );
        mImported[ImportedPath( assetId, kind ).String()] = std::move( bytes );
        return assetId;
    }

    AssetId AddModel( string_view path, const ModelAsset& model )
    {
        return Add( path, WriteModelAsset( model ), cModelKind );
    }

    AssetId AddMaterial( string_view path, string_view json )
    {
        return Add( path, WriteMaterialAsset( *TryReadMaterialAsset( json ) ), cMaterialKind );
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

// A material's own WGSL: a glow that breathes with the world's time.
constexpr string_view cPulse = R"wgsl(struct Material
{
    glow: vec3f, // @color @default( 0.2, 1, 0.6 )
    rate: f32, // @default( 2 )
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    out.albedo = vec3f( 0.05 );
    out.emissive = material.glow * ( 1.0 + sin( in.time * material.rate ) );
    return out;
}
)wgsl";

Scene Level( Project& project )
{
    const AssetId paint =
        project.AddMaterial( "materials/paint.mat", R"({ "params": { "base_color": [ 0.8, 0.15, 0.1, 1 ] } })" );
    const AssetId shinyPaint = project.AddMaterial(
        "materials/shiny_paint.mat",
        format( R"({{ "base": {{ "asset": "{}" }}, "params": {{ "roughness": 0.2 }} }})", paint.ToString() ) );
    const AssetId gold = project.AddMaterial(
        "materials/gold.mat",
        R"({ "params": { "base_color": [ 1, 0.78, 0.35, 1 ], "metallic": 1, "roughness": 0.3 } })" );
    const AssetId glass = project.AddMaterial(
        "materials/glass.mat",
        R"({ "transparent": true, "params": { "base_color": [ 0.6, 0.8, 1, 0.35 ], "roughness": 0.1 } })" );
    const AssetId pulse =
        project.AddMaterial( "materials/pulse.mat",
                             format( R"({{ "shader": {{ "asset": "{}" }} }})",
                                     project.Add( "shaders/pulse.wgsl", string( cPulse ), cShaderKind ).ToString() ) );
    const AssetId grass = project.AddMaterial(
        "materials/grass.mat", R"({ "params": { "base_color": [ 0.45, 0.6, 0.35, 1 ], "roughness": 0.9 } })" );

    Scene scene;
    const Entity ground = CreateChild( scene, scene.Root(), Make( "ground" ) );
    scene.Add<Transform>( ground );
    scene.Add<Model>( ground, Model{ .mAsset = project.AddModel( "models/ground.glb", MakePlane( 20.0f ) ),
                                     .mMaterials = { grass },
                                     .mTint = {} } );

    const AssetId box = project.AddModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    const array<AssetId, 5> materials{ paint, gold, glass, pulse, shinyPaint };
    const array<Transform, 5> boxes{
        Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } },
        Transform{ .mPosition = { 2.5f, 1.0f, -1.0f }, .mRotation = { 0.0f, 0.6f, 0.0f }, .mScale = vec3( 2.0f ) },
        Transform{ .mPosition = { -2.0f, 0.35f, 1.5f }, .mRotation = { 0.0f, 0.3f, 0.0f }, .mScale = vec3( 0.7f ) },
        Transform{ .mPosition = { -1.5f, 1.5f, -2.5f }, .mRotation = { 0.5f, 0.8f, 0.2f }, .mScale = vec3( 1.0f ) },
        Transform{ .mPosition = { 1.0f, 0.25f, 2.5f }, .mScale = { 3.0f, 0.5f, 0.5f } },
    };
    for ( size_t at = 0; at < boxes.size(); ++at )
    {
        const Entity entity = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( entity, boxes[at] );
        scene.Add<Model>( entity, Model{ .mAsset = box, .mMaterials = { materials[at] }, .mTint = {} } );
    }

    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -40.0f ), glm::radians( 35.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
    const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
    scene.Add<Transform>( lamp, Transform{ .mPosition = { -2.0f, 1.5f, 3.0f } } );
    scene.Add<PointLight>( lamp, PointLight{ .mColor = { .mR = 1.0f, .mG = 0.6f, .mB = 0.3f, .mA = 1.0f },
                                             .mIntensity = 15.0f,
                                             .mRange = 8.0f } );
    // Placed each frame by the loop, as the actions turn it.
    const Entity camera = CreateChild( scene, scene.Root(), Make( "camera" ) );
    scene.Add<Transform>( camera );
    scene.Add<Camera>( camera );
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

// The frames to draw before quitting, from `--frames N`; none: until the
// window closes.
opt<u64> TryFramesToDraw( int argc, char** argv )
{
    for ( int at = 1; at + 1 < argc; ++at )
        if ( string_view( argv[at] ) == "--frames" )
        {
            u64 frames = 0;
            const string_view text( argv[at + 1] );
            if ( from_chars( text.data(), text.data() + text.size(), frames ).ec == errc() )
                return frames;
        }
    return nullopt;
}
}

int main( int argc, char** argv )
{
    const opt<u64> framesToDraw = TryFramesToDraw( argc, argv );
    u64 framesDrawn = 0;
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
    Project project;
    App app;
    AssetRegistry assets( project.mCatalog, project.Read(), app.GetJobs() );
    RegisterModelLoader( assets );
    RegisterMaterialLoader( assets );
    RegisterShaderLoader( assets );
    RegisterTextureLoader( assets );
    app.AddModule( TransformModule() );
    app.AddModule( RenderModule( assets ) );
    World world( app, Level( project ) );
    const Entity camera = *TryResolve( world.GetScene(), EntityPath{ Make( "/camera" ) } );

    // The GPU after the window, and gone before it.
    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
    Scope<Renderer> renderer;
    RenderView view;
    Input input( Controls() );
    f32 turn = 2.2f;
    Entity picked;
    platform.Run( [&]( f64 elapsed ) {
        for ( const InputEvent& event : platform.Poll() )
        {
            input.Handle( event );
            // A touch is a share of the window; the target is its pixels.
            if ( const auto* touch = get_if<TouchDown>( &event ); touch and renderer )
                view.Pick( ivec2( touch->mPosition * vec2( window.PixelSize() ) ) );
        }
        if ( renderer )
            if ( const auto pickedMaybe = view.TryPicked() )
            {
                picked = *pickedMaybe;
                LogInfo( "picked {}", Describe( world.GetScene(), picked ) );
            }
        const Actions actions = input.LatchFrame();
        turn += actions.Axis( "move_x"_n ) * static_cast<f32>( elapsed ) * 1.5f;
        world.GetScene().Get<Transform>( camera ) = Orbit( turn );

        // What there is to see by: the world's axes, the picked item's box.
        DebugDraw& debug = world.Get<DebugDraw>();
        debug.Axes( mat4( 1.0f ), 1.5f );
        for ( const RenderItem& item : world.Get<RenderScene>().mItems )
            if ( const auto modelMaybe = item.mModel.TryGet(); modelMaybe and item.mEntity == picked )
                debug.Box( item.mWorld, modelMaybe->mMin, modelMaybe->mMax,
                           Color{ .mR = 1.0f, .mG = 0.85f, .mB = 0.1f, .mA = 1.0f } );
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
                view, scene, scene.mCameras.front(),
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() } );
            surface.Present();
            ++framesDrawn;
        }
#ifndef __EMSCRIPTEN__
        // Minimized: nothing to present, nothing to wait for.
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
#endif
        if ( framesToDraw and framesDrawn >= *framesToDraw )
            return false;
        return not platform.QuitRequested();
    } );
    return 0;
}
