// bubble_editor [project folder] [--open path]... [--play] [--mcp [port]] [--frames N] [--screenshot file.png]
//
// The editor over a project's folder: its sources imported as it opens and
// again as they change, project.bubble's start level open as a document,
// shown in a viewport through the editor's own camera. With no folder it
// starts on the start window - the projects opened before, a folder to open,
// a new project -, and File > Close Project comes back to it.
// --open opens a file of the project too - a level, a prefab, a material -,
// the last one active; --play plays the scene shown, as Play does. --mcp
// serves the editor to agents - Model Context Protocol, on
// http://127.0.0.1:8752/mcp or the port given -: what is open read and
// changed by its tools. --frames quits after N frames, --screenshot writes
// the last one - the whole window - as a PNG.
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/png.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/editor_mcp.hpp"
#include "bubble/editor/editor_preferences.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/project_picker.hpp"
#include "bubble/editor/recent_projects.hpp"
#include "bubble/platform/platform.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "bubble/render/gpu/renderer.hpp"
#include "bubble/types/stream.hpp"
#include "bubble/types/thread.hpp"

using namespace bubble;

namespace
{
struct Arguments
{
    opt<OsPath> mRootMaybe;
    vector<string> mOpen;
    bool mPlay = false;
    opt<u64> mFrames;
    string mScreenshot;
    opt<u16> mMcpPort;
};

expected<Arguments, string> TryArguments( int argc, char** argv )
{
    Arguments arguments;
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
        else if ( argument == "--open" and at + 1 < argc )
            arguments.mOpen.emplace_back( argv[++at] );
        else if ( argument == "--play" )
            arguments.mPlay = true;
        else if ( argument == "--mcp" )
        {
            arguments.mMcpPort = McpServer::cDefaultPort;
            if ( at + 1 < argc and not string_view( argv[at + 1] ).starts_with( "--" ) )
                if ( const auto portMaybe = TryParse<u16>( argv[at + 1] ) )
                {
                    arguments.mMcpPort = *portMaybe;
                    ++at;
                }
        }
        else if ( not arguments.mRootMaybe and not argument.starts_with( "--" ) )
            arguments.mRootMaybe = PathFromUtf8( argument );
        else
            return unexpected( format( "{}: not an argument (bubble_editor [project folder] [--open path]... "
                                       "[--play] [--mcp [port]] [--frames N] [--screenshot file.png])",
                                       argument ) );
    }
    return arguments;
}

// Behind the panels.
constexpr Color cBackground{ .mR = 0.1f, .mG = 0.1f, .mB = 0.11f, .mA = 1.0f };

i64 Now()
{
    return chrono::duration_cast<chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count();
}

// ImGui for the start window or for a project: a context of its own each -
// a project's layout is its own, in its .bubble -, the platform's side, and
// the drawing once the GPU has come. Made again between frames as one gives
// way to the other.
struct Ui
{
    Scope<ImGuiContextScope> mContext;
    Scope<ImGuiInput> mInput;
    Scope<ImGuiRenderer> mRenderer;

    void Reset()
    {
        mRenderer.reset();
        mInput.reset();
        mContext.reset();
    }
};

// A project open: what it is drawn with, the editor over it, and the MCP
// server that reaches into it.
struct Session
{
    Scope<EditorProject> mProject;
    Scope<Renderer> mRenderer;
    Scope<Editor> mEditor;
    // Last, so it goes first: its tools reach into the editor.
    Scope<McpServer> mMcp;
    opt<u16> mMcpTried;
    // Made yet: the editor waits for the GPU.
    bool Ready() const { return mEditor != nullptr; }

    // What reaches into the project first, the project last.
    void Reset()
    {
        mMcp.reset();
        mEditor.reset();
        mRenderer.reset();
        mProject.reset();
        mMcpTried.reset();
    }
};

void WriteScreenshot( Gpu& gpu, ImGuiRenderer& renderer, ImDrawData& data, ivec2 size, const string& file )
{
    const GpuTexture shot( gpu,
                           GpuTextureOptions{ .mSize = size,
                                              .mMips = 1,
                                              .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                              .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                              .mLabel = "screenshot" } );
    renderer.Draw( data, RenderTarget{ .mView = shot.View(), .mSize = shot.Size(), .mFormat = shot.Format() },
                   cBackground );
    if ( const auto pixelsMaybe = gpu.TryReadRgba8( shot ) )
    {
        ofstream( PathFromUtf8( file ), ios::binary ) << EncodePng( *pixelsMaybe, shot.Size() );
        LogInfo( "{} written", file );
    }
    else
        LogError( "screenshot: {}", pixelsMaybe.error() );
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

    const auto platformMaybe = Platform::TryStart();
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
    // One person's preferences and the projects they opened, for every
    // project, in their own folder.
    const auto folderMaybe = platform.TrySaveFolder( "editor" );
    if ( not folderMaybe )
        LogWarning( "preferences: {}; kept until the editor quits alone", folderMaybe.error() );
    Preferences preferences( folderMaybe ? opt<OsPath>( *folderMaybe / Preferences::cFileName ) : nullopt );
    RecentProjects recent( folderMaybe ? opt<OsPath>( *folderMaybe / RecentProjects::cFileName ) : nullopt );

    auto windowMaybe = platform.TryOpenWindow(
        WindowOptions{ .mTitle = "bubble editor", .mSize = { 1600, 900 }, .mResizable = true } );
    if ( not windowMaybe )
    {
        LogError( "{}", windowMaybe.error() );
        return 1;
    }
    Window& window = *windowMaybe;

    const Scope<GpuStartup> startup = Gpu::Start( window );
    Scope<Gpu> gpu;
    Ui ui;
    AppearancePreferences appearance = preferences.Get().mAppearance;
    const auto makeUi = [&]( const OsPath& layoutFile ) {
        ui.Reset();
        ui.mContext = CreateScope<ImGuiContextScope>( layoutFile );
        appearance = preferences.Get().mAppearance;
        Editor::SetUpImGui( appearance, window.Scale() );
        ui.mInput = CreateScope<ImGuiInput>( platform, window );
        if ( gpu )
            ui.mRenderer = CreateScope<ImGuiRenderer>( *gpu );
    };

    Session session;
    // The editor over the project, once the GPU is there.
    const auto makeEditor = [&] {
        session.mRenderer = CreateScope<Renderer>( *gpu, session.mProject->GetAssets() );
        session.mEditor = CreateScope<Editor>( *session.mProject, preferences, *gpu, *session.mRenderer,
                                               [&platform]( const OsPath& path ) {
                                                   if ( auto openedMaybe = platform.TryOpen( path ); not openedMaybe )
                                                       LogError( "{}", openedMaybe.error() );
                                               } );
    };
    // A project's folder opened, its start level shown; the error says why
    // it would not open.
    const auto tryOpen = [&]( const OsPath& root ) -> expected<void, string> {
        auto projectMaybe = EditorProject::TryOpen( root, *audio );
        if ( not projectMaybe )
            return unexpected( std::move( projectMaybe.error() ) );
        session.mProject = std::move( *projectMaybe );
        if ( auto openedMaybe = session.mProject->TryOpenStartLevel(); not openedMaybe )
            LogWarning( "{}", openedMaybe.error() );
        recent.Opened( root, session.mProject->GetSettings().mName, Now() );
        // The layout is the project's, beside its import cache.
        makeUi( root / ".bubble" / "editor_layout.ini" );
        if ( gpu )
            makeEditor();
        return {};
    };
    const auto close = [&] {
        session.Reset();
        makeUi( {} );
        window.SetTitle( "bubble editor" );
    };

    ProjectPicker picker( recent, [&platform, &window]( const OsPath& from, function<void( opt<OsPath> )> chosen ) {
        platform.ChooseFolder( window, from, std::move( chosen ) );
    } );
    if ( arguments.mRootMaybe )
    {
        if ( auto openedMaybe = tryOpen( *arguments.mRootMaybe ); not openedMaybe )
        {
            LogError( "{}", openedMaybe.error() );
            return 1;
        }
        for ( const string& path : arguments.mOpen )
            session.mProject->Run( "document.open",
                                   Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( path ) } } ) );
        if ( arguments.mPlay )
            session.mProject->Run( "play.start" );
    }
    else
        makeUi( {} );

    string title;
    bool cursorLocked = false;
    u64 framesDrawn = 0;
    int exitCode = 0;
    platform.Run( [&]( f64 elapsed ) {
        for ( const InputEvent& event : platform.Poll() )
        {
            audio->Resume();
            ui.mInput->Handle( event );
            // A game playing takes the player's input too, while it has the
            // viewport.
            if ( session.Ready() )
                session.mEditor->Handle( event );
        }
        // The window closed: the editor asks about what is not saved first.
        if ( platform.TakeQuitRequest() )
        {
            if ( not session.Ready() )
                return false;
            session.mEditor->AskToQuit();
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
            ui.mRenderer = CreateScope<ImGuiRenderer>( *gpu );
            if ( session.mProject )
                makeEditor();
        }
        if ( session.Ready() )
        {
            // The MCP server as --mcp or the preferences want it: started,
            // moved to another port, stopped. A port that fails is not tried
            // again until the wish changes.
            const McpPreferences& mcp = preferences.Get().mMcp;
            const opt<u16> wantedMaybe = arguments.mMcpPort ? arguments.mMcpPort
                                         : mcp.mServe       ? opt<u16>( static_cast<u16>( mcp.mPort ) )
                                                            : nullopt;
            if ( wantedMaybe != session.mMcpTried )
            {
                session.mMcpTried = wantedMaybe;
                session.mMcp.reset();
                session.mEditor->SetMcpUrl( "" );
                if ( wantedMaybe )
                {
                    vector<McpTool> tools = EditorMcpTools( *session.mProject );
                    tools.push_back( ViewportScreenshotTool( *gpu, session.mEditor->GetViewport() ) );
                    auto serverMaybe = McpServer::TryStart( *wantedMaybe, std::move( tools ), EditorMcpInstructions() );
                    if ( serverMaybe )
                    {
                        session.mMcp = std::move( *serverMaybe );
                        session.mEditor->SetMcpUrl( session.mMcp->Url() );
                        LogInfo( "MCP server at {}: claude mcp add --transport http bubble_editor {}",
                                 session.mMcp->Url(), session.mMcp->Url() );
                    }
                    else
                        LogError( "{}", serverMaybe.error() );
                }
            }
        }
        gpu->Poll();
        if ( session.Ready() )
        {
            session.mProject->GetApp().GetJobs().RunMainQueue();
            // What agents asked, between frames, as the menus act.
            if ( session.mMcp )
                session.mMcp->Serve();
        }

        // The appearance changed in the preferences: the fonts and the style
        // made again, between frames.
        if ( preferences.Get().mAppearance != appearance )
        {
            appearance = preferences.Get().mAppearance;
            Editor::SetUpImGui( appearance, window.Scale() );
        }
        ui.mInput->NewFrame( elapsed );
        ImGui::NewFrame();
        if ( session.Ready() )
            session.mEditor->Frame( elapsed );
        else
            picker.Show();
        ui.mInput->EndFrame();
        ImGui::Render();
        if ( session.Ready() )
        {
            session.mEditor->Draw();
            if ( const string now = session.mEditor->Title(); now != title )
            {
                title = now;
                window.SetTitle( title );
            }
            if ( const bool locked = session.mEditor->CursorLocked(); locked != cursorLocked )
            {
                cursorLocked = locked;
                window.SetCursorLocked( locked );
            }
        }

        GpuSurface& surface = *gpu->TrySurface();
        surface.Resize( window.PixelSize() );
        ImDrawData& data = *ImGui::GetDrawData();
        if ( auto viewMaybe = surface.TryAcquire() )
        {
            ui.mRenderer->Draw(
                data,
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() },
                cBackground );
            surface.Present();
            ++framesDrawn;
            if ( arguments.mFrames and framesDrawn >= *arguments.mFrames and not arguments.mScreenshot.empty() )
                WriteScreenshot( *gpu, *ui.mRenderer, data, surface.Size(), arguments.mScreenshot );
        }
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
        if ( arguments.mFrames and framesDrawn >= *arguments.mFrames )
            return false;

        // Between frames: a project picked opens, a project closed gives way
        // to the start window.
        if ( session.Ready() )
        {
            if ( session.mEditor->QuitAsked() )
                return false;
            if ( session.mEditor->CloseAsked() )
            {
                if ( cursorLocked )
                {
                    cursorLocked = false;
                    window.SetCursorLocked( false );
                }
                title.clear();
                close();
            }
        }
        else if ( const auto chosenMaybe = picker.TakeChosen() )
        {
            if ( auto openedMaybe = tryOpen( *chosenMaybe ); not openedMaybe )
                picker.Failed( openedMaybe.error() );
        }
        return true;
    } );
    return exitCode;
}
