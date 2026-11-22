// bubble_runner <project.bubble> [--frames N] [--screenshot file.png] [--import-only]
//
// Plays a project - its project.bubble, or the folder it is in -: its sources imported into .bubble/imported
// as it starts and again as they change - the registry reloading them -, its
// project.bubble's start level loaded and played with its actions. --frames
// quits after N frames, --screenshot writes the last one as a PNG, and
// --import-only imports and quits: .import files are written for the sources
// that had none.
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/png.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/project_setup.hpp"
#include "bubble/import/script_importer.hpp"
#include "bubble/input/input.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/scene/engine_file.hpp"
#include "bubble/scene/prefab.hpp"
#include "bubble/scene/scene_file.hpp"
#include "bubble/scripts/script_declarations.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/types/thread.hpp"
#include "bubble/ui/ui.hpp"
#include "bubble/world/game.hpp"
#include "bubble/world/project.hpp"
#include "bubble/world/world.hpp"

using namespace bubble;

namespace
{
struct Arguments
{
    OsPath mRoot;
    opt<u64> mFrames;
    string mScreenshot;
    bool mImportOnly = false;
};

expected<Arguments, string> TryArguments( int argc, char** argv )
{
    Arguments arguments;
    bool rooted = false;
    for ( int at = 1; at < argc; ++at )
    {
        const string_view argument( argv[at] );
        if ( argument == "--frames" and at + 1 < argc )
        {
            u64 frames = 0;
            const string_view text( argv[++at] );
            if ( from_chars( text.data(), text.data() + text.size(), frames ).ec != errc() )
                return unexpected( format( "--frames {}: a whole number of frames", text ) );
            arguments.mFrames = frames;
        }
        else if ( argument == "--screenshot" and at + 1 < argc )
            arguments.mScreenshot = argv[++at];
        else if ( argument == "--import-only" )
            arguments.mImportOnly = true;
        else if ( not rooted and not argument.starts_with( "--" ) )
        {
            // The project's file names its folder.
            arguments.mRoot = PathFromUtf8( argument );
            if ( arguments.mRoot.extension() == ".bubble" )
                arguments.mRoot = arguments.mRoot.parent_path();
            rooted = true;
        }
        else
            return unexpected( format( "{}: not an argument", argument ) );
    }
    if ( not rooted )
        return unexpected( "bubble_runner <project.bubble> [--frames N] [--screenshot file.png] [--import-only]"s );
    return arguments;
}

// What a refresh said, a line each.
void Report( const ProjectRefresh& refresh )
{
    for ( const string& warning : refresh.mWarnings )
        LogWarning( "{}", warning );
    for ( const string& error : refresh.mErrors )
        LogError( "{}", error );
    for ( const auto& [path, assetId] : refresh.mOrphans )
        LogWarning( "{}: an .import whose source is gone", path.View() );
}

// Prefabs as a level's instances need them: read from the project's files
// and parsed, their own instances the same way.
struct ProjectPrefabs
{
    const ProjectFiles& mFiles;
    const ComponentTypes& mComponentTypes;
    const AssetCatalog& mCatalog;

    expected<Ref<const Prefab>, string> operator()( const AssetId& prefabId ) const
    {
        const auto pathMaybe = mCatalog.TryPathOf( prefabId );
        if ( not pathMaybe )
            return unexpected( format( "no prefab {} in the project", prefabId.ToString() ) );
        auto textMaybe = mFiles.TryRead( *pathMaybe );
        if ( not textMaybe )
            return unexpected( format( "{}: {}", pathMaybe->View(), textMaybe.error() ) );
        auto prefabMaybe = Prefab::TryParse( *textMaybe, FileContext( mComponentTypes, mCatalog, *this ) );
        if ( not prefabMaybe )
            return unexpected( format( "{}:{}", pathMaybe->View(), prefabMaybe.error() ) );
        return CreateRef<const Prefab>( std::move( *prefabMaybe ) );
    }
};

expected<Scene, string> TryLoadLevel( const ProjectFiles& files,
                                      const ComponentTypes& componentTypes,
                                      const AssetCatalog& catalog,
                                      const AssetId& levelId )
{
    const auto pathMaybe = catalog.TryPathOf( levelId );
    if ( not pathMaybe )
        return unexpected( format( "no level {} in the project", levelId.ToString() ) );
    auto textMaybe = files.TryRead( *pathMaybe );
    if ( not textMaybe )
        return unexpected( format( "{}: {}", pathMaybe->View(), textMaybe.error() ) );
    auto sceneMaybe = TryLoadScene(
        *textMaybe, FileContext( componentTypes, catalog, ProjectPrefabs{ files, componentTypes, catalog } ) );
    if ( not sceneMaybe )
        return unexpected( format( "{}:{}", pathMaybe->View(), sceneMaybe.error() ) );
    return std::move( *sceneMaybe );
}
}

int main( int argc, char** argv )
{
    const auto argumentsMaybe = TryArguments( argc, argv );
    if ( not argumentsMaybe )
    {
        LogError( "{}", argumentsMaybe.error() );
        return 1;
    }
    const Arguments& arguments = *argumentsMaybe;

    // The project's settings first: its actions are what scripts are
    // checked against.
    ProjectAssets project( ProjectFiles::FromDirectory( arguments.mRoot ), Importers(), cEngineFileExtensions );
    const auto settingsTextMaybe = project.GetFiles().TryRead( *AssetPath::TryFrom( cProjectFile ) );
    if ( not settingsTextMaybe )
    {
        LogError( "{}: {}", cProjectFile, settingsTextMaybe.error() );
        return 1;
    }
    const auto settingsMaybe = TryReadProjectSettings( *settingsTextMaybe );
    if ( not settingsMaybe )
    {
        LogError( "{}:{}", cProjectFile, settingsMaybe.error() );
        return 1;
    }
    const ProjectSettings& settings = *settingsMaybe;

    App app;
    vector<string> actions;
    for ( const Action& action : settings.mActions.List() )
        actions.push_back( action.mName.ToString() );
    AssetRegistry assets( project.GetCatalog(), AssetRegistry::FromDirectory( arguments.mRoot ), app.GetJobs() );
    const auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = {}, .mMouseAsTouch = false } );
    if ( not platformMaybe )
    {
        LogError( "{}", platformMaybe.error() );
        return 1;
    }
    Platform& platform = **platformMaybe;
    auto deviceMaybe = AudioDevice::TryStart();
    if ( not deviceMaybe )
        LogWarning( "{}", deviceMaybe.error() );
    const Scope<AudioDevice> audio = deviceMaybe ? std::move( *deviceMaybe ) : AudioDevice::Offline();
    Ref<SaveSlots> saves;
    const string game = settings.mName.empty() ? "bubble" : settings.mName;
    if ( auto folderMaybe = platform.TrySaveFolder( game ) )
        saves = SaveSlotsInFolder( *folderMaybe, app.GetJobs() );
    hmap<string, string> aliases;
    if ( const auto luaurcMaybe = project.GetFiles().TryRead( *AssetPath::TryFrom( ".luaurc" ) ) )
        if ( auto readMaybe = TryReadLuaurc( *luaurcMaybe ) )
            aliases = std::move( *readMaybe );
    AddEngineModules( app, assets, *audio,
                      ScriptModuleOptions{ .mGlobals = {},
                                           .mAliases = aliases,
                                           .mTimeLimit = 1.0,
                                           .mProgress = CreateRef<ScriptProgress>(),
                                           .mSaves = saves,
                                           .mGameVersion = game } );

    // Scripts are checked strictly against the engine's declarations with the
    // project's actions.
    auto scriptCheckMaybe =
        TryProjectScriptCheck( app, project.GetFiles(), project.GetCatalog(), ScriptLabels{ .mActions = actions } );
    if ( not scriptCheckMaybe )
    {
        LogError( "{}", scriptCheckMaybe.error() );
        return 1;
    }
    // The same declarations under .bubble, out of the scan's sight, for an
    // editor's Luau language server to read; written when they change.
    const AssetPath declarationsPath = *AssetPath::TryFrom( ".bubble/bubble.d.luau" );
    if ( const string declarations = scriptCheckMaybe->mDeclarations;
         project.GetFiles().TryRead( declarationsPath ).value_or( string() ) != declarations )
        if ( auto writtenMaybe = project.GetFiles().TryWrite( declarationsPath, declarations ); not writtenMaybe )
            LogWarning( "{}: {}", declarationsPath.View(), writtenMaybe.error() );
    AddEngineImporters( project, app, std::move( *scriptCheckMaybe ) );
    const auto refreshMaybe = project.TryRefresh();
    if ( not refreshMaybe )
    {
        LogError( "{}", refreshMaybe.error() );
        return 1;
    }
    Report( *refreshMaybe );
    LogInfo( "{}: {} assets, {} imported", PathToUtf8( arguments.mRoot ), project.GetCatalog().Paths().size(),
             refreshMaybe->mImported.size() );
    if ( arguments.mImportOnly )
        return refreshMaybe->mErrors.empty() ? 0 : 1;

    RegisterEngineLoaders( assets );

    const auto levelIdMaybe = settings.TryStartLevel( project.GetCatalog() );
    if ( not levelIdMaybe )
    {
        LogError( "{}: start_level: no such level in the project", cProjectFile );
        return 1;
    }
    auto sceneMaybe = TryLoadLevel( project.GetFiles(), app.GetComponentTypes(), project.GetCatalog(), *levelIdMaybe );
    if ( not sceneMaybe )
    {
        LogError( "{}", sceneMaybe.error() );
        return 1;
    }
    World world( app, std::move( *sceneMaybe ), WorldOptions{ .mLevel = *levelIdMaybe } );

    const auto windowMaybe =
        platform.TryOpenWindow( WindowOptions{ .mTitle = game, .mSize = { 1280, 720 }, .mResizable = true } );
    if ( not windowMaybe )
    {
        LogError( "{}", windowMaybe.error() );
        return 1;
    }
    const Window& window = *windowMaybe;
    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
    Scope<Renderer> renderer;
    RenderView view;
    Input input( settings.mActions );
    world.SetInput( WorldInput{ .mLatchStep = [&] { return input.LatchStep(); },
                                .mLatchFrame = [&] { return input.LatchFrame(); } } );
    u64 framesDrawn = 0;
    f64 sinceLook = 0;
    bool cursorLocked = false;
    int exitCode = 0;
    platform.Run( [&]( f64 elapsed ) {
        // The project's files looked at now and then: what changed imported
        // and reloaded, the game going on.
        sinceLook += elapsed;
        if ( sinceLook > 0.5 )
        {
            sinceLook = 0;
            if ( const auto lookMaybe = project.TryRefresh() )
            {
                Report( *lookMaybe );
                for ( const AssetPath& path : lookMaybe->mImported )
                    if ( auto reloadedMaybe = assets.TryReload( path ); not reloadedMaybe )
                        LogError( "{}", reloadedMaybe.error() );
            }
        }
        UiWorld& ui = world.Get<UiWorld>();
        ui.SetScreen( window.Size() );
        for ( const InputEvent& event : platform.Poll() )
        {
            audio->Resume();
            if ( ui.Take( event ) )
                continue;
            input.Handle( event );
        }
        world.Frame( elapsed );
        app.GetJobs().RunMainQueue();
        const Game& gameState = world.Get<Game>();
        if ( gameState.CursorLocked() != cursorLocked )
        {
            cursorLocked = gameState.CursorLocked();
            window.SetCursorLocked( cursorLocked );
        }

        if ( not gpu )
        {
            auto finishedMaybe = startup->TryFinish();
            if ( not finishedMaybe )
                return true;
            if ( not *finishedMaybe )
            {
                LogError( "{}", finishedMaybe->error() );
                exitCode = 1;
                return false;
            }
            gpu = std::move( **finishedMaybe );
            renderer = CreateScope<Renderer>( *gpu, assets );
        }
        gpu->Poll();
        GpuSurface& surface = *gpu->TrySurface();
        surface.Resize( window.PixelSize() );
        const RenderScene& scene = world.Get<RenderScene>();
        const bool last = arguments.mFrames and framesDrawn + 1 >= *arguments.mFrames;
        if ( auto viewMaybe = surface.TryAcquire(); viewMaybe and not scene.mCameras.empty() )
        {
            renderer->Draw(
                view, scene, scene.mCameras.front(),
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() },
                world.Get<UiScene>() );
            surface.Present();
            ++framesDrawn;
            // The last frame again, into a texture that can be read back.
            if ( last and not arguments.mScreenshot.empty() )
            {
                const GpuTexture shot(
                    *gpu, GpuTextureOptions{ .mSize = surface.Size(),
                                             .mMips = 1,
                                             .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                             .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                             .mLabel = "screenshot" } );
                renderer->Draw( view, scene, scene.mCameras.front(),
                                RenderTarget{ .mView = shot.View(), .mSize = shot.Size(), .mFormat = shot.Format() },
                                world.Get<UiScene>() );
                if ( const auto pixelsMaybe = gpu->TryReadRgba8( shot ) )
                {
                    ofstream( PathFromUtf8( arguments.mScreenshot ), ios::binary )
                        << EncodePng( *pixelsMaybe, shot.Size() );
                    LogInfo( "{} written", arguments.mScreenshot );
                }
                else
                    LogError( "screenshot: {}", pixelsMaybe.error() );
            }
        }
        else if ( scene.mCameras.empty() )
        {
            // No camera: nothing to see, and nothing to wait for.
            ++framesDrawn;
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
        }
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
        if ( arguments.mFrames and framesDrawn >= *arguments.mFrames )
            return false;
        return not platform.QuitRequested() and not gameState.QuitAsked();
    } );
    return exitCode;
}
