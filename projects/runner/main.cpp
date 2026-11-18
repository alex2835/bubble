// The runner: a game on its own. The project and its levels come with
// their stages; for now a scene made here - boxes of a few materials on the
// ground under the sun, a warm lamp - drawn by the renderer and played by
// scripts on physics: the red box walks as "move_x" and "move_z" say and
// jumps with a blip and throws crates - a prefab spawned -, two boxes tumble
// down, the camera follows, and a pad chimes and lights up when
// the box reaches it - remembered in `progress` and written to the
// "autosave" slot, which the next run reads. Stages 5 to 8 checked by hand. The keyboard, a gamepad and touches drive
// the actions; the mouse touches too, for a screen without touch. A touch
// or a click picks what it lands on: its box drawn around it, its path
// logged. `--frames N` quits after N frames drawn: a run a script can check.
#include "bubble/animation/animation.hpp"
#include "bubble/animation/animation_assets.hpp"
#include "bubble/assets/model_primitives.hpp"
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/assets/shader_asset.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/input/input.hpp"
#include "bubble/physics/physics.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/hierarchy.hpp"
#include "bubble/scripts/script_module.hpp"
#include "bubble/types/thread.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/transform.hpp"
#include "bubble/world/world.hpp"

using namespace bubble;

namespace
{
ActionMap Controls()
{
    // The left half of the window walks left and right, the right half
    // forward and back.
    const TouchArea leftQuarter{ .mMin = { 0.0f, 0.0f }, .mMax = { 0.25f, 1.0f } };
    const TouchArea secondQuarter{ .mMin = { 0.25f, 0.0f }, .mMax = { 0.5f, 1.0f } };
    const TouchArea topRight{ .mMin = { 0.5f, 0.0f }, .mMax = { 1.0f, 0.5f } };
    const TouchArea bottomRight{ .mMin = { 0.5f, 0.5f }, .mMax = { 1.0f, 1.0f } };
    ActionMap map;
    map.Bind( "move_x", Key::A, -1.0f )
        .Bind( "move_x", Key::D )
        .Bind( "move_x", Key::Left, -1.0f )
        .Bind( "move_x", Key::Right )
        .Bind( "move_x", GamepadAxis::LeftX )
        .Bind( "move_x", leftQuarter, -1.0f )
        .Bind( "move_x", secondQuarter );
    map.Bind( "move_z", Key::W )
        .Bind( "move_z", Key::S, -1.0f )
        .Bind( "move_z", Key::Up )
        .Bind( "move_z", Key::Down, -1.0f )
        .Bind( "move_z", GamepadAxis::LeftY, -1.0f )
        .Bind( "move_z", topRight )
        .Bind( "move_z", bottomRight, -1.0f );
    map.Bind( "jump", Key::Space ).Bind( "jump", GamepadButton::South );
    map.Bind( "spawn", Key::E ).Bind( "spawn", GamepadButton::West );
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

    // Compiled here as the importer compiles it - progress read each time -
    // but not checked: the runner has no analyzer. Written typed all the
    // same.
    AssetId AddSound( string_view path, const string& wav )
    {
        return Add( path, WriteSoundAsset( wav, false ), cSoundKind );
    }

    AssetId AddScript( string_view path, string_view source )
    {
        auto bytecodeMaybe =
            TryCompileScript( source, CompileOptions{ .mOptimization = 1, .mMutableGlobals = { "progress" } } );
        if ( not bytecodeMaybe )
            throw logic_error( string( path ) + bytecodeMaybe.error() );
        return Add(
            path,
            WriteScriptAsset( ScriptAsset{ .mBytecode = std::move( *bytecodeMaybe ), .mProps = {}, .mRequires = {} } ),
            cScriptKind );
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

// A tone `seconds` long at `frequency`, fading out: a 16-bit mono .wav, the
// runner having no sound files of its own.
string MakeTone( f32 seconds, f32 frequency )
{
    constexpr u32 cRate = 44100;
    const auto frames = static_cast<u32>( seconds * static_cast<f32>( cRate ) );
    const auto put = []( string& out, u32 value, u32 bytes ) {
        for ( u32 byte = 0; byte < bytes; ++byte )
            out.push_back( static_cast<char>( ( value >> ( 8 * byte ) ) & 0xFF ) );
    };
    string samples;
    for ( u32 frame = 0; frame < frames; ++frame )
    {
        const f32 time = static_cast<f32>( frame ) / static_cast<f32>( cRate );
        const f32 fade = 1.0f - time / seconds;
        const f32 value = 0.5f * fade * fade * sin( 2.0f * glm::pi<f32>() * frequency * time );
        put( samples, static_cast<u32>( static_cast<u16>( static_cast<i16>( value * 32767.0f ) ) ), 2 );
    }
    string wav = "RIFF";
    put( wav, 36 + static_cast<u32>( samples.size() ), 4 );
    wav += "WAVEfmt ";
    put( wav, 16, 4 );
    put( wav, 1, 2 );
    put( wav, 1, 2 );
    put( wav, cRate, 4 );
    put( wav, cRate * 2, 4 );
    put( wav, 2, 2 );
    put( wav, 16, 2 );
    wav += "data";
    put( wav, static_cast<u32>( samples.size() ), 4 );
    return wav + samples;
}

// The red box: walks and jumps as the actions say. The jump is a height and
// the time to its top, the two things seen on screen; gravity follows.
constexpr string_view cPlayer = R"luau(
local P = props { speed = 3, jump_height = 1.2, time_to_apex = 0.35, crate = asset(), blip = asset() }
type Self = typeof( P ) & { entity: entity }

function on_fixed_update( self: Self, dt: number )
    local move = vector.create( input.axis( "move_x" ), 0, -input.axis( "move_z" ) )
    if vector.magnitude( move ) > 1 then
        move = vector.normalize( move )
    end
    local body = self.entity.character_controller
    local gravity = 2 * self.jump_height / ( self.time_to_apex * self.time_to_apex )
    local rise = body.velocity.y - gravity * dt
    if body.on_ground then
        rise = 0
        if input.pressed( "jump" ) then
            rise = gravity * self.time_to_apex
            audio.play_sound( self.blip, 0.4 )
        end
    end
    body.velocity = move * self.speed + vector.create( 0, rise, 0 ) + body.ground_velocity
    -- A crate from above, tumbling down beside the player.
    if input.pressed( "spawn" ) then
        local at = self.entity.transform.position + vector.create( math.random() - 0.5, 3, math.random() - 0.5 )
        spawn( self.crate, nil, at )
    end
end
)luau";

// The camera: behind its target and above, looking down at it.
constexpr string_view cFollow = R"luau(
local P = props { target = entity(), offset = vector.create( 0, 6, 9 ) }
type Self = typeof( P ) & { entity: entity }

function on_update( self: Self, dt: number )
    local t = self.entity.transform
    t.position = self.target.transform.position + self.offset
    t.rotation = vector.create( -math.atan2( self.offset.y, self.offset.z ), 0, 0 )
end
)luau";

// The pad: lit once its target has stood on it - in this run, the last
// level's (progress) or an earlier run's (the autosave slot).
// The HUD's button: a crate dropped from above.
constexpr string_view cDropButton = R"luau(
local P = props { crate = asset() }

function on_click( self )
    spawn( self.crate, nil, vector.create( 0, 4, 0 ) )
end
)luau";

// The HUD's slider: how loud the game's sounds are.
constexpr string_view cVolume = R"luau(
function on_change( self, value: number | boolean )
    if type( value ) == "number" then
        audio.set_bus_volume( "sfx", value )
    end
end
)luau";

constexpr string_view cPad = R"luau(
local P = props { target = entity(), reach = 1.2, chime = asset() }
type Self = typeof( P ) & { entity: entity }

local function light_up( self: Self )
    self.entity.model.tint = color( 0.3, 1, 0.4 )
end

function on_start( self: Self )
    local saved = save.read( "autosave" )
    if progress.pad_reached or ( saved and saved.pad_reached ) then
        light_up( self )
    end
end

function on_update( self: Self, dt: number )
    if not progress.pad_reached then
        debug_draw.line( self.entity.transform.position, self.target.transform.position, color( 1, 0.85, 0.1 ) )
    end
end

function on_fixed_update( self: Self, dt: number )
    if progress.pad_reached then
        return
    end
    local apart = self.target.transform.position - self.entity.transform.position
    if vector.magnitude( vector.create( apart.x, 0, apart.z ) ) < self.reach then
        progress.pad_reached = true
        audio.play_sound_at( self.chime, self.entity.transform.position )
        save.write( "autosave", { pad_reached = true } )
        print( "the pad is reached: written to the autosave slot" )
        light_up( self )
    end
end
)luau";

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
    const AssetId plane = project.AddModel( "models/ground.glb", MakePlane( 20.0f ) );
    scene.Add<Model>( ground, Model{ .mAsset = plane, .mMaterials = { grass }, .mTint = {} } );
    scene.Add<MeshCollider>( ground, MeshCollider{ .mModel = plane } );

    const AssetId box = project.AddModel( "models/box.glb", MakeBox( vec3( 1.0f ) ) );
    const array<AssetId, 5> materials{ paint, gold, glass, pulse, shinyPaint };
    const array<Transform, 5> boxes{
        Transform{ .mPosition = { 0.0f, 0.5f, 0.0f } },
        Transform{ .mPosition = { 2.5f, 1.0f, -1.0f }, .mRotation = { 0.0f, 0.6f, 0.0f }, .mScale = vec3( 2.0f ) },
        Transform{ .mPosition = { -2.0f, 0.35f, 1.5f }, .mRotation = { 0.0f, 0.3f, 0.0f }, .mScale = vec3( 0.7f ) },
        Transform{ .mPosition = { -1.5f, 1.5f, -2.5f }, .mRotation = { 0.5f, 0.8f, 0.2f }, .mScale = vec3( 1.0f ) },
        Transform{ .mPosition = { 1.0f, 0.25f, 2.5f }, .mScale = { 3.0f, 0.5f, 0.5f } },
    };
    for ( size_t at = 1; at < boxes.size(); ++at )
    {
        const Entity entity = CreateChild( scene, scene.Root(), Make( "box" ) );
        scene.Add<Transform>( entity, boxes[at] );
        scene.Add<Model>( entity, Model{ .mAsset = box, .mMaterials = { materials[at] }, .mTint = {} } );
        scene.Add<BoxCollider>( entity );
        // The glass and the glowing one fall and tumble; the rest stand.
        if ( at == 2 or at == 3 )
            scene.Add<RigidBody>( entity );
    }
    // The player stands on its feet: the character's origin; its box is a
    // child, half a unit up.
    const Entity player = CreateChild( scene, scene.Root(), Make( "player" ) );
    scene.Add<Transform>( player );
    scene.Add<CharacterController>(
        player, CharacterController{
                    .mRadius = 0.45f, .mHeight = 1.0f, .mMaxSlope = glm::radians( 45.0f ), .mStepHeight = 0.3f } );
    // What E throws: a crate, as a prefab - the player's prop.
    const AssetId crate = project.Add( "prefabs/crate.prefab",
                                       format( R"({{ "version": 1, "next_id": 1, "root": {{ "id": 0, "components": {{
            "transform": {{ "scale": [ 0.6, 0.6, 0.6 ] }},
            "model": {{ "asset": {{ "asset": "{}" }}, "materials": [ {{ "asset": "{}" }} ] }},
            "box_collider": {{}}, "rigid_body": {{}} }} }} }})",
                                               box.ToString(), gold.ToString() ),
                                       cPrefabKind );
    scene.Add<Script>(
        player,
        Script{ .mAsset = project.AddScript( "scripts/player.luau", cPlayer ),
                .mProps = Value( Value::Record{
                    ValueField{ Name::Declare( "crate" ), Value( crate ) },
                    ValueField{ Name::Declare( "blip" ),
                                Value( project.AddSound( "sounds/blip.wav", MakeTone( 0.12f, 880.0f ) ) ) } } ) } );
    const Entity body = CreateChild( scene, player, Make( "body" ) );
    scene.Add<Transform>( body, boxes[0] );
    scene.Add<Model>( body, Model{ .mAsset = box, .mMaterials = { materials[0] }, .mTint = {} } );
    const Value target(
        Value::Record{ ValueField{ Name::Declare( "target" ), Value( EntityPath{ Make( "/player" ) } ) } } );

    const Entity pad = CreateChild( scene, scene.Root(), Make( "pad" ) );
    scene.Add<Transform>( pad, Transform{ .mPosition = { 3.5f, 0.03f, 3.0f }, .mScale = { 1.5f, 0.06f, 1.5f } } );
    scene.Add<Model>( pad, Model{ .mAsset = box, .mMaterials = { grass }, .mTint = {} } );
    scene.Add<BoxCollider>( pad );
    const Value padProps(
        Value::Record{ ValueField{ Name::Declare( "target" ), Value( EntityPath{ Make( "/player" ) } ) },
                       ValueField{ Name::Declare( "chime" ),
                                   Value( project.AddSound( "sounds/chime.wav", MakeTone( 0.8f, 660.0f ) ) ) } } );
    scene.Add<Script>( pad, Script{ .mAsset = project.AddScript( "scripts/pad.luau", cPad ), .mProps = padProps } );

    const Entity sun = CreateChild( scene, scene.Root(), Make( "sun" ) );
    scene.Add<Transform>( sun, Transform{ .mRotation = { glm::radians( -40.0f ), glm::radians( 35.0f ), 0.0f } } );
    scene.Add<DirectionalLight>( sun );
    const Entity lamp = CreateChild( scene, scene.Root(), Make( "lamp" ) );
    scene.Add<Transform>( lamp, Transform{ .mPosition = { -2.0f, 1.5f, 3.0f } } );
    scene.Add<PointLight>( lamp, PointLight{ .mColor = { .mR = 1.0f, .mG = 0.6f, .mB = 0.3f, .mA = 1.0f },
                                             .mIntensity = 15.0f,
                                             .mRange = 8.0f } );
    // The HUD: a panel in the top left corner with a button that drops a
    // crate and a slider for the sounds' volume. The arrows stay the
    // game's.
    const Entity hud = CreateChild( scene, scene.Root(), Make( "hud" ) );
    scene.Add<UiCanvas>( hud, UiCanvas{ .mWidth = 960, .mHeight = 540, .mNavigation = false } );
    const Entity panel = CreateChild( scene, hud, Make( "panel" ) );
    UiRect panelRect;
    panelRect.mPosition = Name::Declare( "absolute" );
    panelRect.mLeft = "16";
    panelRect.mTop = "16";
    panelRect.mPadding = 8;
    panelRect.mGap = 12;
    panelRect.mDirection = Name::Declare( "row" );
    panelRect.mAlign = Name::Declare( "center" );
    scene.Add<UiRect>( panel, panelRect );
    scene.Add<UiImage>( panel, UiImage{ .mImage = AssetId{}, .mColor = { 0.1f, 0.12f, 0.16f, 0.75f }, .mSlice = 0 } );
    const Entity drop = CreateChild( scene, panel, Make( "drop" ) );
    UiRect dropRect;
    dropRect.mWidth = "40";
    dropRect.mHeight = "40";
    scene.Add<UiRect>( drop, dropRect );
    scene.Add<UiImage>( drop, UiImage{ .mImage = AssetId{}, .mColor = { 0.95f, 0.65f, 0.2f, 1.0f }, .mSlice = 0 } );
    scene.Add<UiButton>( drop );
    scene.Add<Script>(
        drop, Script{ .mAsset = project.AddScript( "scripts/drop_button.luau", cDropButton ),
                      .mProps = Value( Value::Record{ ValueField{ Name::Declare( "crate" ), Value( crate ) } } ) } );
    const Entity volume = CreateChild( scene, panel, Make( "volume" ) );
    UiRect volumeRect;
    volumeRect.mWidth = "160";
    volumeRect.mHeight = "12";
    scene.Add<UiRect>( volume, volumeRect );
    scene.Add<UiImage>( volume, UiImage{ .mImage = AssetId{}, .mColor = { 0.35f, 0.38f, 0.45f, 1.0f }, .mSlice = 0 } );
    scene.Add<UiSlider>( volume, UiSlider{ .mValue = 1, .mMin = 0, .mMax = 1 } );
    scene.Add<Script>( volume, Script{ .mAsset = project.AddScript( "scripts/volume.luau", cVolume ) } );

    // Placed each frame by its script, behind the player.
    const Entity camera = CreateChild( scene, scene.Root(), Make( "camera" ) );
    scene.Add<Transform>( camera );
    scene.Add<Camera>( camera );
    scene.Add<Script>( camera,
                       Script{ .mAsset = project.AddScript( "scripts/follow.luau", cFollow ), .mProps = target } );
    return scene;
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
    RegisterScriptLoader( assets );
    RegisterPrefabLoader( assets );
    RegisterSoundLoader( assets );
    RegisterAnimationLoaders( assets );
    RegisterFontLoader( assets );
    // The system's sound; with none the game plays on, silent.
    auto deviceMaybe = AudioDevice::TryStart();
    if ( not deviceMaybe )
        LogWarning( "{}", deviceMaybe.error() );
    const Scope<AudioDevice> audio = deviceMaybe ? std::move( *deviceMaybe ) : AudioDevice::Offline();
    // Saves in the user's folder; without one the game plays on, unsaved.
    Ref<SaveSlots> saves;
    if ( auto folderMaybe = platform.TrySaveFolder( "runner" ) )
        saves = SaveSlotsInFolder( *folderMaybe, app.GetJobs() );
    else
        LogWarning( "{}", folderMaybe.error() );
    app.AddModule( TransformModule() );
    app.AddModule( ScriptModule( assets, ScriptModuleOptions{ .mGlobals = {},
                                                              .mAliases = {},
                                                              .mTimeLimit = 1.0,
                                                              .mProgress = CreateRef<ScriptProgress>(),
                                                              .mSaves = saves,
                                                              .mGameVersion = "runner 0.7" } ) );
    app.AddModule( PhysicsModule( assets ) );
    app.AddModule( AudioModule( assets, *audio ) );
    app.AddModule( AnimationModule( assets ) );
    app.AddModule( UiModule( assets ) );
    app.AddModule( RenderModule( assets ) );
    World world( app, Level( project ) );

    // The GPU after the window, and gone before it.
    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
    Scope<Renderer> renderer;
    RenderView view;
    Input input( Controls() );
    world.SetInput( WorldInput{ .mLatchStep = [&] { return input.LatchStep(); },
                                .mLatchFrame = [&] { return input.LatchFrame(); } } );
    Entity picked;
    platform.Run( [&]( f64 elapsed ) {
        // The UI's screen is the window, in the units of its pointer.
        UiWorld& ui = world.Get<UiWorld>();
        ui.SetScreen( window.Size() );
        for ( const InputEvent& event : platform.Poll() )
        {
            // A browser plays sound once the page saw a gesture.
            audio->Resume();
            // What the UI takes the game does not see.
            if ( ui.Take( event ) )
                continue;
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
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() },
                world.Get<UiScene>() );
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
