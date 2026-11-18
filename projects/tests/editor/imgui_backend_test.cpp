// ImGui's backends of our own: the platform's events going into ImGui's
// queue as it takes them, and its draw data - its own texture, ours - drawn
// by the GPU. Not on the web, where node has no window and no WebGPU.
#ifndef __EMSCRIPTEN__
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>
#include <imgui_internal.h>

using namespace bubble;

namespace
{
// What ImGui has queued to take at its next frame, an event a line.
vector<string> Queued()
{
    vector<string> lines;
    for ( const ImGuiInputEvent& event : ImGui::GetCurrentContext()->InputEventsQueue )
        switch ( event.Type )
        {
            case ImGuiInputEventType_Key:
                lines.push_back(
                    format( "key {} {}", ImGui::GetKeyName( event.Key.Key ), event.Key.Down ? "down" : "up" ) );
                break;
            case ImGuiInputEventType_Text: lines.push_back( format( "text {:x}", event.Text.Char ) ); break;
            case ImGuiInputEventType_MousePos:
                lines.push_back( format( "pos {} {}", event.MousePos.PosX, event.MousePos.PosY ) );
                break;
            case ImGuiInputEventType_MouseButton:
                lines.push_back(
                    format( "button {} {}", event.MouseButton.Button, event.MouseButton.Down ? "down" : "up" ) );
                break;
            case ImGuiInputEventType_MouseWheel:
                lines.push_back( format( "wheel {} {}", event.MouseWheel.WheelX, event.MouseWheel.WheelY ) );
                break;
            default: lines.push_back( "other" ); break;
        }
    return lines;
}
}

TEST_CASE( "The platform's events go to ImGui as its own: keys with their modifiers, text, the mouse" )
{
    auto platformMaybe = Platform::TryStart( PlatformOptions{ .mVideoDriver = "dummy", .mMouseAsTouch = false } );
    REQUIRE_MESSAGE( platformMaybe, platformMaybe.error() );
    auto windowMaybe = ( *platformMaybe )->TryOpenWindow( WindowOptions{ .mTitle = "imgui", .mSize = { 64, 64 } } );
    REQUIRE_MESSAGE( windowMaybe, windowMaybe.error() );
    const ImGuiContextScope context;
    ImGuiInput input( **platformMaybe, *windowMaybe );

    input.Handle( KeyDown{ .mKey = Key::LeftCtrl, .mRepeat = false } );
    input.Handle( KeyDown{ .mKey = Key::A, .mRepeat = false } );
    input.Handle( KeyUp{ .mKey = Key::LeftCtrl } );
    // A key ImGui has no name for goes nowhere.
    input.Handle( KeyDown{ .mKey = Key::Unknown, .mRepeat = false } );
    input.Handle( TextInput{ .mText = "жA" } );
    input.Handle( MouseMove{ .mPosition = { 10.0f, 20.0f }, .mDelta = { 1.0f, 1.0f } } );
    input.Handle( MouseDown{ .mButton = MouseButton::Right, .mPosition = { 10.0f, 20.0f } } );
    input.Handle( MouseUp{ .mButton = MouseButton::Right, .mPosition = { 12.0f, 20.0f } } );
    // Up when it was not down: nothing happened.
    input.Handle( MouseUp{ .mButton = MouseButton::Middle, .mPosition = { 12.0f, 20.0f } } );
    input.Handle( MouseWheel{ .mDelta = { 1.0f, 2.0f } } );
    CHECK( Queued() == vector<string>{ "key ModCtrl down", "key LeftCtrl down", "key A down", "key ModCtrl up",
                                       "key LeftCtrl up", "text 436", "text 41", "pos 10 20", "button 1 down",
                                       "pos 12 20", "button 1 up", "wheel -1 2" } );

    input.NewFrame( 0.0 );
    CHECK( ImGui::GetIO().DisplaySize.x == 64.0f );
    CHECK( ImGui::GetIO().DeltaTime > 0.0f );

    // The clipboard is the platform's.
    ImGui::SetClipboardText( "буфер" );
    CHECK( ( *platformMaybe )->ClipboardText() == "буфер" );
    CHECK( string( ImGui::GetClipboardText() ) == "буфер" );
}

TEST_CASE( "ImGui's lists are drawn with its font atlas, made and grown as it asks, and with textures of ours" )
{
    const Scope<Gpu> gpu = StartGpu();
    const ImGuiContextScope context;
    Editor::SetUpImGui( 1.0f );
    ImGuiRenderer renderer( *gpu );
    const GpuTexture target( *gpu,
                             GpuTextureOptions{ .mSize = ivec2( 64 ),
                                                .mMips = 1,
                                                .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                                .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                                .mLabel = "target" } );
    const GpuTexture green( *gpu, GpuTextureOptions{ .mSize = ivec2( 2 ), .mMips = 1, .mLabel = "green" } );
    const array<u8, 16> texels{ 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
    green.Write( *gpu, 0, AsBytes( texels ) );

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2( 64, 64 );
    const auto frame = [&]( const function<void( ImDrawList& )>& draw ) {
        io.DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        draw( *ImGui::GetForegroundDrawList() );
        ImGui::Render();
        renderer.Draw( *ImGui::GetDrawData(),
                       RenderTarget{ .mView = target.View(), .mSize = target.Size(), .mFormat = target.Format() },
                       Color{ .mR = 0, .mG = 0, .mB = 0, .mA = 1 } );
        for ( const ImTextureData* texture : ImGui::GetPlatformIO().Textures )
        {
            CHECK( texture->Status == ImTextureStatus_OK );
            CHECK( texture->TexID != ImTextureID_Invalid );
        }
        return Required( gpu->TryReadRgba8( target ) );
    };

    const vector<u8> pixels = frame( [&]( ImDrawList& list ) {
        list.AddRectFilled( ImVec2( 8, 8 ), ImVec2( 32, 56 ), IM_COL32( 255, 0, 0, 255 ) );
        list.AddImage( ImGuiRenderer::TextureId( green.View() ), ImVec2( 32, 8 ), ImVec2( 56, 56 ) );
    } );
    CHECK( PixelAt( pixels, 64, 16, 32 ) == "255 0 0 255" );
    CHECK( PixelAt( pixels, 64, 44, 32 ) == "0 255 0 255" );
    CHECK( PixelAt( pixels, 64, 2, 2 ) == "0 0 0 255" );

    // Text the atlas has no glyphs for yet: they are made, the atlas sent
    // again, and drawn.
    const vector<u8> text = frame( [&]( ImDrawList& list ) {
        list.AddRectFilled( ImVec2( 0, 0 ), ImVec2( 64, 64 ), IM_COL32( 0, 0, 255, 255 ) );
        list.AddText( ImGui::GetFont(), 40.0f, ImVec2( 4, 4 ), IM_COL32( 255, 255, 255, 255 ), "Жж" );
    } );
    u32 lit = 0;
    for ( size_t at = 0; at < text.size(); at += 4 )
        lit += text[at] > 200 and text[at + 1] > 200 ? 1u : 0u;
    CHECK( lit > 50 );
}
#endif
