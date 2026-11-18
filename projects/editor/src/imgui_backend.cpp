#include "bubble/editor/imgui_backend.hpp"
#include "bubble/core/color.hpp"
#include "bubble/core/utf8.hpp"
#include "bubble/render/gpu/gpu_binding.hpp"
#include "bubble/render/gpu/gpu_pipeline.hpp"
#include "bubble/types/algorithm.hpp"
#include "bubble/types/format.hpp"
#include <glm/common.hpp>

namespace bubble
{
namespace
{
// ImGui's own vertices: corners in its display's units, the atlas's UV, a
// colour in sRGB as four bytes. Its textures are linear data - the atlas is
// white with coverage in alpha - or ours, sampled as what they are.
constexpr string_view cShader = R"wgsl(
struct Frame
{
    to_clip: mat4x4f,
}

@group( 0 ) @binding( 0 ) var<uniform> frame: Frame;
@group( 0 ) @binding( 1 ) var image_sampler: sampler;
@group( 1 ) @binding( 0 ) var image: texture_2d<f32>;

struct Vertex
{
    @location( 0 ) position: vec2f,
    @location( 1 ) uv: vec2f,
    @location( 2 ) color: vec4f,
}

struct Varyings
{
    @builtin( position ) clip: vec4f,
    @location( 0 ) uv: vec2f,
    @location( 1 ) color: vec4f,
}

fn to_linear( srgb: vec3f ) -> vec3f
{
    let low = srgb / 12.92;
    let high = pow( ( srgb + 0.055 ) / 1.055, vec3f( 2.4 ) );
    return select( high, low, srgb <= vec3f( 0.04045 ) );
}

@vertex
fn vs_main( vertex: Vertex ) -> Varyings
{
    var out: Varyings;
    out.clip = frame.to_clip * vec4f( vertex.position, 0.0, 1.0 );
    out.uv = vertex.uv;
    out.color = vec4f( to_linear( vertex.color.rgb ), vertex.color.a );
    return out;
}

@fragment
fn fs_main( in: Varyings ) -> @location( 0 ) vec4f
{
    return in.color * textureSample( image, image_sampler, in.uv );
}
)wgsl";

static_assert( sizeof( ImDrawVert ) == 20 and sizeof( ImDrawIdx ) == 2 );

ImGuiKey KeyOf( Key key )
{
    const u16 code = static_cast<u16>( key );
    if ( key >= Key::A and key <= Key::Z )
        return static_cast<ImGuiKey>( ImGuiKey_A + ( code - static_cast<u16>( Key::A ) ) );
    // Digit1 comes first, Digit0 last; ImGui has 0 first.
    if ( key >= Key::Digit1 and key <= Key::Digit9 )
        return static_cast<ImGuiKey>( ImGuiKey_1 + ( code - static_cast<u16>( Key::Digit1 ) ) );
    if ( key >= Key::F1 and key <= Key::F12 )
        return static_cast<ImGuiKey>( ImGuiKey_F1 + ( code - static_cast<u16>( Key::F1 ) ) );
    if ( key >= Key::Keypad1 and key <= Key::Keypad9 )
        return static_cast<ImGuiKey>( ImGuiKey_Keypad1 + ( code - static_cast<u16>( Key::Keypad1 ) ) );
    switch ( key )
    {
        case Key::Digit0: return ImGuiKey_0;
        case Key::Enter: return ImGuiKey_Enter;
        case Key::Escape: return ImGuiKey_Escape;
        case Key::Backspace: return ImGuiKey_Backspace;
        case Key::Tab: return ImGuiKey_Tab;
        case Key::Space: return ImGuiKey_Space;
        case Key::Minus: return ImGuiKey_Minus;
        case Key::Equals: return ImGuiKey_Equal;
        case Key::LeftBracket: return ImGuiKey_LeftBracket;
        case Key::RightBracket: return ImGuiKey_RightBracket;
        case Key::Backslash: return ImGuiKey_Backslash;
        case Key::Semicolon: return ImGuiKey_Semicolon;
        case Key::Apostrophe: return ImGuiKey_Apostrophe;
        case Key::Grave: return ImGuiKey_GraveAccent;
        case Key::Comma: return ImGuiKey_Comma;
        case Key::Period: return ImGuiKey_Period;
        case Key::Slash: return ImGuiKey_Slash;
        case Key::CapsLock: return ImGuiKey_CapsLock;
        case Key::PrintScreen: return ImGuiKey_PrintScreen;
        case Key::ScrollLock: return ImGuiKey_ScrollLock;
        case Key::Pause: return ImGuiKey_Pause;
        case Key::Insert: return ImGuiKey_Insert;
        case Key::Home: return ImGuiKey_Home;
        case Key::PageUp: return ImGuiKey_PageUp;
        case Key::Delete: return ImGuiKey_Delete;
        case Key::End: return ImGuiKey_End;
        case Key::PageDown: return ImGuiKey_PageDown;
        case Key::Right: return ImGuiKey_RightArrow;
        case Key::Left: return ImGuiKey_LeftArrow;
        case Key::Down: return ImGuiKey_DownArrow;
        case Key::Up: return ImGuiKey_UpArrow;
        case Key::NumLock: return ImGuiKey_NumLock;
        case Key::KeypadDivide: return ImGuiKey_KeypadDivide;
        case Key::KeypadMultiply: return ImGuiKey_KeypadMultiply;
        case Key::KeypadMinus: return ImGuiKey_KeypadSubtract;
        case Key::KeypadPlus: return ImGuiKey_KeypadAdd;
        case Key::KeypadEnter: return ImGuiKey_KeypadEnter;
        case Key::Keypad0: return ImGuiKey_Keypad0;
        case Key::KeypadPeriod: return ImGuiKey_KeypadDecimal;
        case Key::Menu: return ImGuiKey_Menu;
        case Key::LeftCtrl: return ImGuiKey_LeftCtrl;
        case Key::LeftShift: return ImGuiKey_LeftShift;
        case Key::LeftAlt: return ImGuiKey_LeftAlt;
        case Key::LeftSuper: return ImGuiKey_LeftSuper;
        case Key::RightCtrl: return ImGuiKey_RightCtrl;
        case Key::RightShift: return ImGuiKey_RightShift;
        case Key::RightAlt: return ImGuiKey_RightAlt;
        case Key::RightSuper: return ImGuiKey_RightSuper;
        default: return ImGuiKey_None;
    }
}

// ImGui's buttons: left, right, middle, then the two side ones.
int ButtonOf( MouseButton button )
{
    switch ( button )
    {
        case MouseButton::Left: return 0;
        case MouseButton::Right: return 1;
        case MouseButton::Middle: return 2;
        case MouseButton::X1: return 3;
        case MouseButton::X2: return 4;
    }
    return 0;
}

Cursor CursorOf( ImGuiMouseCursor cursor )
{
    switch ( cursor )
    {
        case ImGuiMouseCursor_TextInput: return Cursor::Text;
        case ImGuiMouseCursor_ResizeAll: return Cursor::Move;
        case ImGuiMouseCursor_ResizeNS: return Cursor::ResizeNS;
        case ImGuiMouseCursor_ResizeEW: return Cursor::ResizeEW;
        case ImGuiMouseCursor_ResizeNESW: return Cursor::ResizeNESW;
        case ImGuiMouseCursor_ResizeNWSE: return Cursor::ResizeNWSE;
        case ImGuiMouseCursor_Hand: return Cursor::Hand;
        case ImGuiMouseCursor_NotAllowed: return Cursor::NotAllowed;
        case ImGuiMouseCursor_None: return Cursor::Hidden;
        default: return Cursor::Arrow;
    }
}
}

ImGuiContextScope::ImGuiContextScope( const OsPath& layoutFile ) : mLayoutFile( PathToUtf8( layoutFile ) )
{
    mContext = ImGui::CreateContext();
    ImGui::SetCurrentContext( mContext );
    ImGui::GetIO().IniFilename = mLayoutFile.empty() ? nullptr : mLayoutFile.c_str();
}

ImGuiContextScope::~ImGuiContextScope()
{
    ImGui::DestroyContext( mContext );
}

ImGuiInput::ImGuiInput( Platform& platform, const Window& window ) : mPlatform( platform ), mWindow( window )
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "bubble";
    io.BackendPlatformUserData = this;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
    platformIo.Platform_ClipboardUserData = this;
    platformIo.Platform_GetClipboardTextFn = []( ImGuiContext* ) -> const char* {
        auto& input = *static_cast<ImGuiInput*>( ImGui::GetPlatformIO().Platform_ClipboardUserData );
        input.mClipboard = input.mPlatform.ClipboardText();
        return input.mClipboard.c_str();
    };
    platformIo.Platform_SetClipboardTextFn = []( ImGuiContext*, const char* text ) {
        auto& input = *static_cast<ImGuiInput*>( ImGui::GetPlatformIO().Platform_ClipboardUserData );
        input.mPlatform.SetClipboardText( text );
    };
}

ImGuiInput::~ImGuiInput()
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
    ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
    platformIo.Platform_ClipboardUserData = nullptr;
    platformIo.Platform_GetClipboardTextFn = nullptr;
    platformIo.Platform_SetClipboardTextFn = nullptr;
    mWindow.SetTextInput( false );
}

void ImGuiInput::Modifier( ImGuiKey modifier, Key left, Key right )
{
    ImGui::GetIO().AddKeyEvent( modifier, mKeys[static_cast<u16>( left )] or mKeys[static_cast<u16>( right )] );
}

void ImGuiInput::Handle( const InputEvent& event )
{
    ImGuiIO& io = ImGui::GetIO();
    if ( const auto* down = get_if<KeyDown>( &event ) )
    {
        mKeys[static_cast<u16>( down->mKey )] = true;
        Modifier( ImGuiMod_Ctrl, Key::LeftCtrl, Key::RightCtrl );
        Modifier( ImGuiMod_Shift, Key::LeftShift, Key::RightShift );
        Modifier( ImGuiMod_Alt, Key::LeftAlt, Key::RightAlt );
        Modifier( ImGuiMod_Super, Key::LeftSuper, Key::RightSuper );
        if ( const ImGuiKey key = KeyOf( down->mKey ); key != ImGuiKey_None )
            io.AddKeyEvent( key, true );
    }
    else if ( const auto* up = get_if<KeyUp>( &event ) )
    {
        mKeys[static_cast<u16>( up->mKey )] = false;
        Modifier( ImGuiMod_Ctrl, Key::LeftCtrl, Key::RightCtrl );
        Modifier( ImGuiMod_Shift, Key::LeftShift, Key::RightShift );
        Modifier( ImGuiMod_Alt, Key::LeftAlt, Key::RightAlt );
        Modifier( ImGuiMod_Super, Key::LeftSuper, Key::RightSuper );
        if ( const ImGuiKey key = KeyOf( up->mKey ); key != ImGuiKey_None )
            io.AddKeyEvent( key, false );
    }
    else if ( const auto* text = get_if<TextInput>( &event ) )
        io.AddInputCharactersUTF8( text->mText.c_str() );
    else if ( const auto* move = get_if<MouseMove>( &event ) )
        io.AddMousePosEvent( move->mPosition.x, move->mPosition.y );
    else if ( const auto* press = get_if<MouseDown>( &event ) )
    {
        io.AddMousePosEvent( press->mPosition.x, press->mPosition.y );
        io.AddMouseButtonEvent( ButtonOf( press->mButton ), true );
    }
    else if ( const auto* release = get_if<MouseUp>( &event ) )
    {
        io.AddMousePosEvent( release->mPosition.x, release->mPosition.y );
        io.AddMouseButtonEvent( ButtonOf( release->mButton ), false );
    }
    // Ours is right-positive; ImGui scrolls left for a positive x.
    else if ( const auto* wheel = get_if<MouseWheel>( &event ) )
        io.AddMouseWheelEvent( -wheel->mDelta.x, wheel->mDelta.y );
}

void ImGuiInput::NewFrame( f64 elapsed )
{
    ImGuiIO& io = ImGui::GetIO();
    const vec2 size( mWindow.Size() );
    const vec2 pixels( mWindow.PixelSize() );
    io.DisplaySize = ImVec2( size.x, size.y );
    io.DisplayFramebufferScale =
        size.x > 0 and size.y > 0 ? ImVec2( pixels.x / size.x, pixels.y / size.y ) : ImVec2( 1, 1 );
    // ImGui wants time to go forward, a little at least.
    io.DeltaTime = static_cast<f32>( max( elapsed, 1e-4 ) );
}

void ImGuiInput::EndFrame()
{
    const ImGuiIO& io = ImGui::GetIO();
    mWindow.SetTextInput( io.WantTextInput );
    if ( not( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange ) )
        mPlatform.SetCursor( io.MouseDrawCursor ? Cursor::Hidden : CursorOf( ImGui::GetMouseCursor() ) );
}

ImGuiRenderer::ImGuiRenderer( Gpu& gpu ) : mGpu( gpu )
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "bubble";
    io.BackendRendererUserData = this;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

    mShader = CreateShader( gpu, cShader, "imgui" );
    mFrameLayout = CreateLayout(
        gpu, "imgui frame",
        vector<WGPUBindGroupLayoutEntry>{ BufferEntry( 0, WGPUShaderStage_Vertex, WGPUBufferBindingType_Uniform ),
                                          SamplerEntry( 1, WGPUSamplerBindingType_Filtering ) } );
    mImageLayout = CreateLayout( gpu, "imgui image",
                                 vector<WGPUBindGroupLayoutEntry>{ TextureEntry( 0, WGPUTextureSampleType_Float ) } );
    WGPUSamplerDescriptor sampler = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler.label = ToGpuString( "imgui" );
    sampler.magFilter = WGPUFilterMode_Linear;
    sampler.minFilter = WGPUFilterMode_Linear;
    mSampler = GpuSamplerHandle( wgpuDeviceCreateSampler( gpu.Device(), &sampler ) );
    mUniforms = GpuBuffer( gpu, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, sizeof( mat4 ), "imgui frame" );
    mFrameGroup =
        CreateGroup( gpu, "imgui frame", mFrameLayout.Get(),
                     vector<WGPUBindGroupEntry>{ BufferBinding( 0, mUniforms ), SamplerBinding( 1, mSampler.Get() ) } );
}

ImGuiRenderer::~ImGuiRenderer()
{
    // ImGui's textures are ours to let go; it makes them again for a
    // renderer that comes after.
    for ( ImTextureData* texture : ImGui::GetPlatformIO().Textures )
        if ( texture->RefCount == 1 )
        {
            texture->SetTexID( ImTextureID_Invalid );
            texture->SetStatus( ImTextureStatus_Destroyed );
        }
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~( ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures );
}

// A view by its address, which ImGui keeps as a number of 64 bits on every
// platform - the web's addresses are 32.
WGPURenderPipeline ImGuiRenderer::PipelineFor( WGPUTextureFormat format )
{
    GpuRenderPipelineHandle& pipeline = mPipelines[static_cast<u32>( format )];
    if ( pipeline )
        return pipeline.Get();
    const auto attribute = []( WGPUVertexFormat vertexFormat, u64 offset, u32 location ) {
        WGPUVertexAttribute made = WGPU_VERTEX_ATTRIBUTE_INIT;
        made.format = vertexFormat;
        made.offset = offset;
        made.shaderLocation = location;
        return made;
    };
    pipeline = CreateRenderPipeline(
        mGpu, GpuPipelineOptions{ .mLabel = "imgui",
                                  .mShader = mShader.Get(),
                                  .mVertexEntry = "vs_main",
                                  .mFragmentEntry = "fs_main",
                                  .mBindGroupLayouts = { mFrameLayout.Get(), mImageLayout.Get() },
                                  .mVertexBuffers = { GpuVertexBufferLayout{
                                      .mStride = sizeof( ImDrawVert ),
                                      .mAttributes = { attribute( WGPUVertexFormat_Float32x2, 0, 0 ),
                                                       attribute( WGPUVertexFormat_Float32x2, 8, 1 ),
                                                       attribute( WGPUVertexFormat_Unorm8x4, 16, 2 ) } } },
                                  .mColorFormat = format,
                                  .mDepth = nullopt,
                                  .mTopology = WGPUPrimitiveTopology_TriangleList,
                                  .mCullMode = WGPUCullMode_None,
                                  .mBlend = true } );
    return pipeline.Get();
}

ImTextureID ImGuiRenderer::TextureId( WGPUTextureView view )
{
    return static_cast<ImTextureID>( reinterpret_cast<uintptr_t>( view ) );
}

void ImGuiRenderer::Update( ImTextureData& texture )
{
    if ( texture.Status == ImTextureStatus_WantDestroy )
    {
        // Kept while a frame still on its way may draw with it.
        if ( texture.UnusedFrames == 0 )
            return;
        mManaged.erase( texture.GetTexID() );
        texture.SetTexID( ImTextureID_Invalid );
        texture.SetStatus( ImTextureStatus_Destroyed );
        return;
    }
    if ( texture.Format != ImTextureFormat_RGBA32 )
        throw logic_error( "ImGuiRenderer: ImGui's textures are RGBA32" );
    const span<const byte> pixels( static_cast<const byte*>( texture.GetPixels() ),
                                   static_cast<size_t>( texture.GetSizeInBytes() ) );
    if ( texture.Status == ImTextureStatus_WantCreate )
    {
        Managed made;
        made.mTexture =
            GpuTexture( mGpu, GpuTextureOptions{ .mSize = ivec2( texture.Width, texture.Height ),
                                                 .mMips = 1,
                                                 .mFormat = WGPUTextureFormat_RGBA8Unorm,
                                                 .mUsage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
                                                 .mLabel = "imgui texture" } );
        made.mGroup = CreateGroup( mGpu, "imgui image", mImageLayout.Get(),
                                   vector<WGPUBindGroupEntry>{ TextureBinding( 0, made.mTexture.View() ) } );
        made.mTexture.Write( mGpu, 0, pixels );
        const ImTextureID id = TextureId( made.mTexture.View() );
        mManaged.insert_or_assign( id, std::move( made ) );
        texture.SetTexID( id );
    }
    // A few glyphs added: the whole of it again, which is rare and small.
    else if ( texture.Status == ImTextureStatus_WantUpdates )
        mManaged.at( texture.GetTexID() ).mTexture.Write( mGpu, 0, pixels );
    texture.SetStatus( ImTextureStatus_OK );
}

WGPUBindGroup ImGuiRenderer::GroupFor( ImTextureID id )
{
    if ( const auto managed = mManaged.find( id ); managed != mManaged.end() )
        return managed->second.mGroup.Get();
    auto [group, made] = mFrameGroups.try_emplace( id );
    if ( made )
        group->second = CreateGroup( mGpu, "imgui image", mImageLayout.Get(),
                                     vector<WGPUBindGroupEntry>{ TextureBinding(
                                         0, reinterpret_cast<WGPUTextureView>( static_cast<uintptr_t>( id ) ) ) } );
    return group->second.Get();
}

void ImGuiRenderer::Fit( GpuBuffer& buffer, u64 size, WGPUBufferUsage usage, string_view label )
{
    if ( buffer and buffer.Size() >= size )
        return;
    // Room to grow before the next time.
    buffer = GpuBuffer( mGpu, usage | WGPUBufferUsage_CopyDst, max<u64>( size * 2, 4096 ), label );
}

void ImGuiRenderer::Draw( ImDrawData& data, const RenderTarget& target, Color clear )
{
    if ( data.Textures != nullptr )
        for ( ImTextureData* texture : *data.Textures )
            if ( texture->Status != ImTextureStatus_OK )
                Update( *texture );
    mFrameGroups.clear();

    // Every list's vertices and indices in one buffer each.
    mVertexData.clear();
    mIndexData.clear();
    for ( const ImDrawList* list : data.CmdLists )
    {
        mVertexData.insert( mVertexData.end(), list->VtxBuffer.begin(), list->VtxBuffer.end() );
        mIndexData.insert( mIndexData.end(), list->IdxBuffer.begin(), list->IdxBuffer.end() );
    }
    // The queue writes in fours.
    if ( mIndexData.size() % 2 != 0 )
        mIndexData.push_back( 0 );
    if ( not mVertexData.empty() )
    {
        Fit( mVertices, mVertexData.size() * sizeof( ImDrawVert ), WGPUBufferUsage_Vertex, "imgui vertices" );
        Fit( mIndices, mIndexData.size() * sizeof( ImDrawIdx ), WGPUBufferUsage_Index, "imgui indices" );
        mVertices.Write( mGpu, 0, AsBytes( mVertexData ) );
        mIndices.Write( mGpu, 0, AsBytes( mIndexData ) );
    }

    // ImGui's display, its units from the top left, to the clip's.
    const f32 left = data.DisplayPos.x;
    const f32 right = data.DisplayPos.x + data.DisplaySize.x;
    const f32 top = data.DisplayPos.y;
    const f32 bottom = data.DisplayPos.y + data.DisplaySize.y;
    mat4 toClip( 1.0f );
    toClip[0][0] = 2.0f / ( right - left );
    toClip[1][1] = 2.0f / ( top - bottom );
    toClip[2][2] = 0.5f;
    toClip[3][0] = ( right + left ) / ( left - right );
    toClip[3][1] = ( top + bottom ) / ( bottom - top );
    toClip[3][2] = 0.5f;
    mUniforms.Write( mGpu, 0, AsBytes( toClip ) );

    const GpuCommandEncoderHandle encoder( wgpuDeviceCreateCommandEncoder( mGpu.Device(), nullptr ) );
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = target.mView;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    const vec4 linear = ToLinear( clear );
    color.clearValue = WGPUColor{ linear.r, linear.g, linear.b, linear.a };
    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.label = ToGpuString( "imgui" );
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &color;
    const GpuRenderPassHandle pass( wgpuCommandEncoderBeginRenderPass( encoder.Get(), &descriptor ) );
    const ivec2 size = target.mSize;
    if ( not mVertexData.empty() and size.x > 0 and size.y > 0 )
    {
        const WGPURenderPipeline pipeline = PipelineFor( target.mFormat );
        const auto setState = [&] {
            wgpuRenderPassEncoderSetPipeline( pass.Get(), pipeline );
            wgpuRenderPassEncoderSetBindGroup( pass.Get(), 0, mFrameGroup.Get(), 0, nullptr );
            wgpuRenderPassEncoderSetVertexBuffer( pass.Get(), 0, mVertices.Get(), 0, mVertices.Size() );
            wgpuRenderPassEncoderSetIndexBuffer( pass.Get(), mIndices.Get(), WGPUIndexFormat_Uint16, 0,
                                                 mIndices.Size() );
            wgpuRenderPassEncoderSetViewport( pass.Get(), 0, 0, static_cast<f32>( size.x ), static_cast<f32>( size.y ),
                                              0, 1 );
        };
        setState();
        const vec2 scale( data.FramebufferScale.x, data.FramebufferScale.y );
        const vec2 origin( data.DisplayPos.x, data.DisplayPos.y );
        u32 firstVertex = 0;
        u32 firstIndex = 0;
        for ( const ImDrawList* list : data.CmdLists )
        {
            for ( const ImDrawCmd& command : list->CmdBuffer )
            {
                if ( command.UserCallback != nullptr )
                {
                    if ( command.UserCallback == ImDrawCallback_ResetRenderState )
                        setState();
                    else
                        command.UserCallback( list, &command );
                    continue;
                }
                // The clip rectangle in the target's pixels, inside it.
                const vec2 low = glm::clamp( ( vec2( command.ClipRect.x, command.ClipRect.y ) - origin ) * scale,
                                             vec2( 0.0f ), vec2( size ) );
                const vec2 high = glm::clamp( ( vec2( command.ClipRect.z, command.ClipRect.w ) - origin ) * scale,
                                              vec2( 0.0f ), vec2( size ) );
                if ( high.x <= low.x or high.y <= low.y )
                    continue;
                wgpuRenderPassEncoderSetScissorRect( pass.Get(), static_cast<u32>( low.x ), static_cast<u32>( low.y ),
                                                     static_cast<u32>( high.x - low.x ),
                                                     static_cast<u32>( high.y - low.y ) );
                wgpuRenderPassEncoderSetBindGroup( pass.Get(), 1, GroupFor( command.GetTexID() ), 0, nullptr );
                wgpuRenderPassEncoderDrawIndexed( pass.Get(), command.ElemCount, 1, firstIndex + command.IdxOffset,
                                                  static_cast<i32>( firstVertex + command.VtxOffset ), 0 );
            }
            firstVertex += static_cast<u32>( list->VtxBuffer.size() );
            firstIndex += static_cast<u32>( list->IdxBuffer.size() );
        }
    }
    wgpuRenderPassEncoderEnd( pass.Get() );
    mGpu.Submit( encoder );
}
}
