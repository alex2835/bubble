// bubble_editor <project folder> [--open path]... [--play] [--frames N] [--screenshot file.png]
//
// The editor over a project's folder: its sources imported as it opens and
// again as they change, project.bubble's start level open as a document,
// shown in a viewport through the editor's own camera. --open opens a file of
// the project too - a level, a prefab, a material -, the last one active;
// --play plays the scene shown, as Play does.
// --frames quits after N frames, --screenshot writes the last one - the
// whole window - as a PNG.
#include "bubble/audio/audio.hpp"
#include "bubble/core/log.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/data/png.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
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
    OsPath mRoot;
    vector<string> mOpen;
    bool mPlay = false;
    opt<u64> mFrames;
    string mScreenshot;
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
        else if ( argument == "--open" and at + 1 < argc )
            arguments.mOpen.emplace_back( argv[++at] );
        else if ( argument == "--play" )
            arguments.mPlay = true;
        else if ( not rooted and not argument.starts_with( "--" ) )
        {
            arguments.mRoot = PathFromUtf8( argument );
            rooted = true;
        }
        else
            return unexpected( format( "{}: not an argument", argument ) );
    }
    if ( not rooted )
        return unexpected(
            "bubble_editor <project folder> [--open path]... [--play] [--frames N] [--screenshot file.png]"s );
    return arguments;
}

// Behind the panels.
constexpr Color cBackground{ .mR = 0.1f, .mG = 0.1f, .mB = 0.11f, .mA = 1.0f };

// What there is once the GPU has come.
struct Drawing
{
    Scope<Gpu> mGpu;
    Scope<Renderer> mRenderer;
    Scope<ImGuiRenderer> mImGui;
    Scope<Editor> mEditor;
};

void WriteScreenshot( Drawing& drawing, ImDrawData& data, ivec2 size, const string& file )
{
    const GpuTexture shot( *drawing.mGpu,
                           GpuTextureOptions{ .mSize = size,
                                              .mMips = 1,
                                              .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                              .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                              .mLabel = "screenshot" } );
    drawing.mImGui->Draw( data, RenderTarget{ .mView = shot.View(), .mSize = shot.Size(), .mFormat = shot.Format() },
                          cBackground );
    if ( const auto pixelsMaybe = drawing.mGpu->TryReadRgba8( shot ) )
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

    auto projectMaybe = EditorProject::TryOpen( arguments.mRoot, *audio );
    if ( not projectMaybe )
    {
        LogError( "{}", projectMaybe.error() );
        return 1;
    }
    EditorProject& project = **projectMaybe;
    if ( auto openedMaybe = project.TryOpenStartLevel(); not openedMaybe )
        LogWarning( "{}", openedMaybe.error() );
    for ( const string& path : arguments.mOpen )
        project.Run( "document.open", Json( Json::Object{ Json::Member{ .mKey = "path", .mValue = Json( path ) } } ) );
    if ( arguments.mPlay )
        project.Run( "play.start" );

    auto windowMaybe = platform.TryOpenWindow(
        WindowOptions{ .mTitle = "bubble editor", .mSize = { 1600, 900 }, .mResizable = true } );
    if ( not windowMaybe )
    {
        LogError( "{}", windowMaybe.error() );
        return 1;
    }
    Window& window = *windowMaybe;
    // The layout is the project's, beside its import cache.
    const ImGuiContextScope context( arguments.mRoot / ".bubble" / "editor_layout.ini" );
    Editor::SetUpImGui( window.Scale() );
    ImGuiInput input( platform, window );

    const Scope<GpuStartup> startup = Gpu::Start( window );
    Drawing drawing;
    string title;
    bool cursorLocked = false;
    u64 framesDrawn = 0;
    int exitCode = 0;
    platform.Run( [&]( f64 elapsed ) {
        for ( const InputEvent& event : platform.Poll() )
        {
            audio->Resume();
            input.Handle( event );
            // A game playing takes the player's input too, while it has the
            // viewport.
            if ( drawing.mEditor )
                drawing.mEditor->Handle( event );
        }
        if ( not drawing.mGpu )
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
            drawing.mGpu = std::move( **finishedMaybe );
            drawing.mRenderer = CreateScope<Renderer>( *drawing.mGpu, project.GetAssets() );
            drawing.mImGui = CreateScope<ImGuiRenderer>( *drawing.mGpu );
            drawing.mEditor =
                CreateScope<Editor>( project, *drawing.mGpu, *drawing.mRenderer, [&platform]( const OsPath& path ) {
                    if ( auto openedMaybe = platform.TryOpen( path ); not openedMaybe )
                        LogError( "{}", openedMaybe.error() );
                } );
        }
        Gpu& gpu = *drawing.mGpu;
        gpu.Poll();
        project.GetApp().GetJobs().RunMainQueue();

        input.NewFrame( elapsed );
        ImGui::NewFrame();
        drawing.mEditor->Frame( elapsed );
        input.EndFrame();
        ImGui::Render();
        drawing.mEditor->Draw();
        if ( const string now = drawing.mEditor->Title(); now != title )
        {
            title = now;
            window.SetTitle( title );
        }
        if ( const bool locked = drawing.mEditor->CursorLocked(); locked != cursorLocked )
        {
            cursorLocked = locked;
            window.SetCursorLocked( locked );
        }

        GpuSurface& surface = *gpu.TrySurface();
        surface.Resize( window.PixelSize() );
        ImDrawData& data = *ImGui::GetDrawData();
        if ( auto viewMaybe = surface.TryAcquire() )
        {
            drawing.mImGui->Draw(
                data,
                RenderTarget{ .mView = viewMaybe->Get(), .mSize = surface.Size(), .mFormat = surface.ViewFormat() },
                cBackground );
            surface.Present();
            ++framesDrawn;
            if ( arguments.mFrames and framesDrawn >= *arguments.mFrames and not arguments.mScreenshot.empty() )
                WriteScreenshot( drawing, data, surface.Size(), arguments.mScreenshot );
        }
        else
            this_thread::sleep_for( chrono::milliseconds( 10 ) );
        if ( arguments.mFrames and framesDrawn >= *arguments.mFrames )
            return false;
        return not platform.QuitRequested() and not drawing.mEditor->QuitAsked();
    } );
    return exitCode;
}
