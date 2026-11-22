// A material's document in the editor: its preview - a sphere wearing the
// material as the document has it, its bases and its shader followed as
// they load - and the inspector's rows of its flags and its shader's
// parameters, edited with the mouse as ImGui's events.
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor.hpp"
#include "bubble/editor/imgui_backend.hpp"
#include "bubble/editor/inspector.hpp"
#include "bubble/editor/material_preview.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>
#include <imgui.h>

using namespace bubble;

namespace
{
constexpr string_view cProject = R"({ "version": 1, "name": "materials", "actions": {} })";

// The glow shader's material - metal - is rust's base: rust takes its shader.
constexpr string_view cGlow = R"(struct Material
{
    tint: vec3f, // @color @default( 1, 0, 0 )
    strength: f32, // @default( 2 )
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    out.albedo = material.tint * material.strength;
    return out;
}
)";
constexpr string_view cGlowImport = R"({ "version": 1, "asset": "91000000910000009100000091000000", "settings": {} })";

constexpr string_view cMetal = R"({
  "version": 1,
  "shader": { "asset": "91000000910000009100000091000000", "path": "glow.wgsl" },
  "double_sided": true,
  "params": { "strength": 3 }
})";
constexpr string_view cMetalImport = R"({ "version": 1, "asset": "3e7a13e7a13e7a13e7a13e7a13e7a100", "settings": {} })";

constexpr string_view cRust = R"({
  "version": 1,
  "base": { "asset": "3e7a13e7a13e7a13e7a13e7a13e7a100", "path": "metal.mat" },
  "params": { "tint": [ 0.6, 0.3, 0.1 ] }
})";
constexpr string_view cRustImport = R"({ "version": 1, "asset": "40570000405700004057000040570000", "settings": {} })";

constexpr string_view cStone = R"({
  "version": 1,
  "params": { "roughness": 0.8 }
})";
constexpr string_view cStoneImport = R"({ "version": 1, "asset": "57000000570000005700000057000000", "settings": {} })";

struct Materials
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },       { "glow.wgsl", string( cGlow ) },
                                 { "glow.wgsl.import", string( cGlowImport ) },  { "metal.mat", string( cMetal ) },
                                 { "metal.mat.import", string( cMetalImport ) }, { "rust.mat", string( cRust ) },
                                 { "rust.mat.import", string( cRustImport ) },   { "stone.mat", string( cStone ) },
                                 { "stone.mat.import", string( cStoneImport ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;

    Materials()
    {
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
    }

    bool Run( string_view id, string_view args = "null" ) { return mProject->Run( id, *Json::TryParse( args ) ); }

    void Open( string_view path ) { REQUIRE( Run( "document.open", format( R"({{ "path": "{}" }})", path ) ) ); }

    DataDocument& Data() { return *mProject->GetWorkspace().TryActiveData(); }

    MaterialPreview& Preview()
    {
        MaterialPreview& preview = PreviewOf( *mProject, Data() );
        preview.Refresh( Data() );
        return preview;
    }

    // Refreshed until its bases and its shader have come, or failed.
    MaterialPreview& Loaded()
    {
        for ( int frame = 0; frame < 100; ++frame )
        {
            mProject->GetApp().GetJobs().RunMainQueue();
            if ( MaterialPreview& preview = Preview(); preview.TryLayout() or not preview.Error().empty() )
                return preview;
        }
        FAIL( "the material's shader never came" );
        return Preview();
    }

    // What is at `path` in the open material, as JSON text; "-" for nothing.
    string At( string_view path )
    {
        const auto atMaybe = Data().TryAt( *FieldPath::TryParse( path ) );
        if ( not atMaybe )
            return "-";
        string text = atMaybe->Write();
        while ( text.ends_with( '\n' ) )
            text.pop_back();
        return text;
    }
};
}

TEST_CASE( "A material's preview takes its root base's shader and its bases' values" )
{
    Materials f;
    f.Open( "rust.mat" );
    const MaterialPreview& preview = f.Loaded();
    CHECK( preview.Error() == "" );
    REQUIRE( preview.Bases().size() == 1 );
    CHECK( preview.Bases()[0].mDoubleSided == true );
    REQUIRE( preview.TryLayout() );
    CHECK( preview.TryLayout()->TryFind( "tint" ) );
    CHECK( preview.TryLayout()->TryFind( "strength" ) );
    CHECK_FALSE( preview.TryLayout()->TryFind( "roughness" ) );

    // One with no shader and no base: the standard material's parameters.
    f.Open( "stone.mat" );
    const MaterialPreview& stone = f.Loaded();
    CHECK( stone.Bases().empty() );
    REQUIRE( stone.TryLayout() );
    CHECK( stone.TryLayout()->TryFind( "roughness" ) );
    CHECK( stone.TryLayout()->TryFind( "base_color_map" ) );
}

TEST_CASE( "A material's preview says why it does not read or its bases go round; undone, it reads again" )
{
    Materials f;
    f.Open( "rust.mat" );
    REQUIRE( f.Loaded().Error() == "" );

    REQUIRE( f.Run( "data.set", R"({ "path": "double_sided", "value": "yes" })" ) );
    CHECK( f.Preview().Error().starts_with( "rust.mat: " ) );
    CHECK( f.Preview().Error().ends_with( R"(double_sided: true or false, not "yes")" ) );
    CHECK_FALSE( f.Preview().TryMaterial() );
    CHECK_FALSE( f.Preview().TryLayout() );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.Loaded().Error() == "" );
    CHECK( f.Preview().TryMaterial() );

    REQUIRE( f.Run( "data.set", R"({ "path": "base",
                                     "value": { "$asset": { "asset": "40570000405700004057000040570000",
                                                            "path": "rust.mat" } } })" ) );
    CHECK( f.Loaded().Error() == "rust.mat: its bases go round, back to rust.mat" );
    REQUIRE( f.Run( "edit.undo" ) );
    CHECK( f.Loaded().Error() == "" );
}

namespace
{
struct Ui : Materials
{
    ImGuiContextScope mContext;
    Scope<Inspector> mInspector;

    explicit Ui( string_view path )
    {
        mInspector = CreateScope<Inspector>( *mProject );
        Editor::SetUpImGui( AppearancePreferences{}, 1.0f );
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        ImGui::GetIO().DisplaySize = ImVec2( 500, 900 );
        Open( path );
        for ( int frame = 0; frame < 100 and not mInspector->TryWhere( "params.roughness" ); ++frame )
            Frame();
    }

    void Frame()
    {
        mProject->GetApp().GetJobs().RunMainQueue();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        mInspector->Settle();
        ImGui::SetNextWindowPos( ImVec2( 0, 0 ) );
        ImGui::SetNextWindowSize( ImVec2( 500, 900 ) );
        ImGui::Begin( "Inspector" );
        mInspector->Show();
        ImGui::End();
        ImGui::Render();
    }

    vec2 Where( string_view field )
    {
        const auto whereMaybe = mInspector->TryWhere( field );
        REQUIRE_MESSAGE( whereMaybe, field );
        return *whereMaybe;
    }

    void MoveTo( vec2 at )
    {
        ImGui::GetIO().AddMousePosEvent( at.x, at.y );
        Frame();
    }

    void Button( bool down )
    {
        ImGui::GetIO().AddMouseButtonEvent( 0, down );
        Frame();
    }
};
}

TEST_CASE( "The inspector edits a material's parameters and flags: a drag is one step of its document" )
{
    Ui ui( "stone.mat" );
    CHECK( ui.At( "params.roughness" ) == "0.8" );
    CHECK( ui.mInspector->TryWhere( "params.base_color" ) );
    CHECK( ui.mInspector->TryWhere( "params.base_color_map" ) );

    const vec2 from = ui.Where( "params.roughness" );
    ui.MoveTo( from );
    ui.Button( true );
    for ( int step = 1; step <= 5; ++step )
        ui.MoveTo( from + vec2( 8.0f * static_cast<f32>( step ), 0.0f ) );
    ui.Button( false );
    ui.Frame();
    CHECK( ui.At( "params.roughness" ) != "0.8" );
    REQUIRE( ui.Run( "edit.undo" ) );
    CHECK( ui.At( "params.roughness" ) == "0.8" );
    CHECK_FALSE( ui.Data().CanUndo() );

    // A flag the material does not set, clicked: set, the document's own.
    CHECK( ui.At( "double_sided" ) == "-" );
    ui.MoveTo( ui.Where( "double_sided" ) );
    ui.Button( true );
    ui.Button( false );
    ui.Frame();
    CHECK( ui.At( "double_sided" ) == "true" );
    CHECK( ui.Data().IsDirty() );
    // Never written until it is saved.
    CHECK( ui.mFiles.at( "stone.mat" ) == cStone );
}

TEST_CASE( "The preview draws a sphere wearing the material as its document has it, saved or not" )
{
    Materials f;
    Scope<Gpu> gpu = StartGpu();
    Scope<Renderer> renderer = CreateScope<Renderer>( *gpu, f.mProject->GetAssets() );
    RenderView view;
    constexpr i32 cSize = 48;
    GpuTexture target( *gpu, GpuTextureOptions{ .mSize = ivec2( cSize ),
                                                .mMips = 1,
                                                .mFormat = WGPUTextureFormat_RGBA8UnormSrgb,
                                                .mUsage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
                                                .mLabel = "preview" } );
    const auto draw = [&] {
        for ( int frame = 0; frame < 8; ++frame )
        {
            f.mProject->GetApp().GetJobs().RunMainQueue();
            MaterialPreview& preview = f.Preview();
            preview.Get().Frame( 1.0 / 60 );
            Required( gpu->TryCheckNow( [&] {
                renderer->Draw(
                    view, preview.Get().Get<RenderScene>(), preview.Camera().ToRenderCamera(),
                    RenderTarget{ .mView = target.View(), .mSize = target.Size(), .mFormat = target.Format() } );
            } ) );
            gpu->Poll();
        }
        const vector<u8> pixels = Required( gpu->TryReadRgba8( target ) );
        const size_t at = ( static_cast<size_t>( cSize / 2 ) * cSize + cSize / 2 ) * 4;
        return vec3( pixels[at], pixels[at + 1], pixels[at + 2] );
    };

    f.Open( "stone.mat" );
    REQUIRE( f.Run( "data.set", R"({ "path": "params.base_color", "value": [ 1, 0, 0, 1 ] })" ) );
    const vec3 red = draw();
    CHECK_MESSAGE( red.r > red.g + 40, red.r, " ", red.g, " ", red.b );
    CHECK( red.r > red.b + 40 );

    REQUIRE( f.Run( "data.set", R"({ "path": "params.base_color", "value": [ 0, 0, 1, 1 ] })" ) );
    const vec3 blue = draw();
    CHECK_MESSAGE( blue.b > blue.r + 40, blue.r, " ", blue.g, " ", blue.b );
    CHECK( f.mFiles.at( "stone.mat" ) == cStone );

    // The Renderer's copies go before the Gpu.
    renderer.reset();
}
