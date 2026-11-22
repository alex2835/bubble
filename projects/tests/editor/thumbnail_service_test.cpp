// The pictures of a project's assets: a model and a material drawn a few a
// frame, kept in the project's .bubble/thumbnails and read from there the
// next time, drawn again when the asset changes; nothing for what is not
// drawn yet or cannot be.
#include "bubble/assets/prefab_asset.hpp"
#include "bubble/audio/audio.hpp"
#include "bubble/editor/editor_project.hpp"
#include "bubble/editor/thumbnail_service.hpp"
#include "bubble/render/gpu/gpu.hpp"
#include "render/gpu_helpers.hpp"
#include <doctest.h>

using namespace bubble;

#if defined( BUBBLE_IMPORT_MODELS ) and not defined( __EMSCRIPTEN__ )
namespace
{
constexpr string_view cProject =
    R"({ "version": 1, "name": "pictures", "start_level": { "path": "main.scene" }, "actions": {} })";
constexpr string_view cLevel = R"({ "version": 1, "root": { "children": [] } })";

// A unit cube, its faces turned outward.
constexpr string_view cCube = R"(v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
f 1 4 3 2
f 5 6 7 8
f 1 5 8 4
f 2 3 7 6
f 1 2 6 5
f 4 8 7 3
)";
constexpr string_view cCubeImport = R"({ "version": 1, "asset": "c0be0000c0be0000c0be0000c0be0000", "settings": {} })";
constexpr string_view cPaintImport = R"({ "version": 1, "asset": "da1700000da1700000da170000da1700", "settings": {} })";

// A prefab of the cube in the paint, turned and lifted.
constexpr string_view cCrate = R"({
  "version": 1,
  "asset": "c4a7e000c4a7e000c4a7e000c4a7e000",
  "next_id": 1,
  "root": { "id": 0, "components": {
    "transform": { "position": [ 0, 1, 0 ], "rotation": [ 0, 0.5, 0 ] },
    "model": { "asset": { "asset": "c0be0000c0be0000c0be0000c0be0000", "path": "cube.obj" },
               "materials": [ { "asset": "da1700000da1700000da170000da1700", "path": "paint.mat" } ] } } }
})";

AssetId IdOf( string_view text )
{
    return *AssetId::TryParse( text );
}

struct Pictures
{
    hmap<string, string> mFiles{ { "project.bubble", string( cProject ) },
                                 { "main.scene", string( cLevel ) },
                                 { "cube.obj", string( cCube ) },
                                 { "cube.obj.import", string( cCubeImport ) },
                                 { "paint.mat", R"({ "version": 1, "params": { "base_color": [ 1, 0, 0, 1 ] } })" },
                                 { "paint.mat.import", string( cPaintImport ) },
                                 { "crate.prefab", string( cCrate ) } };
    Scope<AudioDevice> mAudio = AudioDevice::Offline();
    Scope<EditorProject> mProject;
    Scope<Gpu> mGpu = StartGpu();
    Scope<Renderer> mRenderer;
    AssetId mCube = IdOf( "c0be0000c0be0000c0be0000c0be0000" );
    AssetId mPaint = IdOf( "da1700000da1700000da170000da1700" );
    AssetId mCrate = IdOf( "c4a7e000c4a7e000c4a7e000c4a7e000" );

    Pictures()
    {
        const ProjectFiles files = ProjectFiles::InMemory( mFiles );
        auto projectMaybe =
            EditorProject::TryOpen( files, AssetRegistry::AtOnce( [files]( const AssetId& assetId, string_view kind ) {
                                        return files.TryRead( ImportedPath( assetId, kind ) );
                                    } ),
                                    *mAudio );
        REQUIRE_MESSAGE( projectMaybe, projectMaybe.error() );
        mProject = std::move( *projectMaybe );
        mRenderer = CreateScope<Renderer>( *mGpu, mProject->GetAssets() );
    }

    ~Pictures() { mRenderer.reset(); }

    // Pumped until it has nothing waiting, or a hundred times.
    void Settle( ThumbnailService& service )
    {
        for ( int frame = 0; frame < 100 and service.Waiting() > 0; ++frame )
        {
            mProject->GetApp().GetJobs().RunMainQueue();
            mGpu->Poll();
            service.Pump();
        }
    }

    // The picture's pixel at (x, y), RGBA.
    array<int, 4> Pixel( const GpuTexture& picture, u32 x, u32 y )
    {
        const auto pixelsMaybe = mGpu->TryReadRgba8( picture );
        REQUIRE_MESSAGE( pixelsMaybe, pixelsMaybe.error() );
        const size_t at = ( size_t{ y } * ThumbnailService::cSize + x ) * 4;
        const vector<u8>& pixels = *pixelsMaybe;
        return { pixels[at], pixels[at + 1], pixels[at + 2], pixels[at + 3] };
    }
};

constexpr u32 cMiddle = ThumbnailService::cSize / 2;
}

TEST_CASE( "A model's picture is drawn in a later frame, the model in the middle of it, the corners the backdrop" )
{
    Pictures pictures;
    ThumbnailService service( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
    const ThumbnailHandle handle = service.Request( pictures.mCube, cModelKind );
    CHECK( service.Request( pictures.mCube, cModelKind ) == handle );
    CHECK_FALSE( service.TryGet( handle ) );
    pictures.Settle( service );

    const auto pictureMaybe = service.TryGet( handle );
    REQUIRE( pictureMaybe );
    const array<int, 4> middle = pictures.Pixel( *pictureMaybe, cMiddle, cMiddle );
    const array<int, 4> corner = pictures.Pixel( *pictureMaybe, 2, 2 );
    CHECK( std::abs( middle[0] - corner[0] ) + std::abs( middle[1] - corner[1] ) > 20 );
    CHECK( service.Drawn() == 1 );
}

TEST_CASE( "A material's picture is a ball in it; one drawn again when the material changes" )
{
    Pictures pictures;
    ThumbnailService service( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
    const ThumbnailHandle handle = service.Request( pictures.mPaint, cMaterialKind );
    pictures.Settle( service );
    REQUIRE( service.TryGet( handle ) );
    const array<int, 4> red = pictures.Pixel( *service.TryGet( handle ), cMiddle, cMiddle );
    CHECK( red[0] > red[1] + 40 );
    CHECK( red[0] > red[2] + 40 );

    pictures.mFiles["paint.mat"] = R"({ "version": 1, "params": { "base_color": [ 0, 0, 1, 1 ] } })";
    pictures.mProject->Refresh();
    pictures.mProject->GetApp().GetJobs().RunMainQueue();
    pictures.Settle( service );
    const array<int, 4> blue = pictures.Pixel( *service.TryGet( handle ), cMiddle, cMiddle );
    CHECK( blue[2] > blue[0] + 40 );
    CHECK( service.Drawn() == 2 );
}

TEST_CASE(
    "A prefab's picture is it put together - its model in its material -, drawn again when the material changes" )
{
    Pictures pictures;
    ThumbnailService service( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
    const ThumbnailHandle handle = service.Request( pictures.mCrate, cPrefabKind );
    pictures.Settle( service );
    REQUIRE( service.TryGet( handle ) );
    const array<int, 4> red = pictures.Pixel( *service.TryGet( handle ), cMiddle, cMiddle );
    CHECK( red[0] > red[1] + 40 );
    // Put together and taken apart: nothing of it is left in the studio.
    const array<int, 4> corner = pictures.Pixel( *service.TryGet( handle ), 2, 2 );
    CHECK( std::abs( corner[0] - corner[2] ) < 10 );

    pictures.mFiles["paint.mat"] = R"({ "version": 1, "params": { "base_color": [ 0, 0, 1, 1 ] } })";
    pictures.mProject->Refresh();
    pictures.mProject->GetApp().GetJobs().RunMainQueue();
    pictures.Settle( service );
    const array<int, 4> blue = pictures.Pixel( *service.TryGet( handle ), cMiddle, cMiddle );
    CHECK( blue[2] > blue[0] + 40 );
}

TEST_CASE( "Pictures drawn once are read back from the project's cache by the next editor, not drawn again" )
{
    Pictures pictures;
    {
        ThumbnailService first( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
        first.Request( pictures.mCube, cModelKind );
        first.Request( pictures.mPaint, cMaterialKind );
        pictures.Settle( first );
        CHECK( first.Drawn() == 2 );
    }
    CHECK( pictures.mFiles.contains( ".bubble/thumbnails/" + pictures.mCube.ToString() + ".thumb" ) );
    ThumbnailService next( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
    const ThumbnailHandle cube = next.Request( pictures.mCube, cModelKind );
    const ThumbnailHandle paint = next.Request( pictures.mPaint, cMaterialKind );
    pictures.Settle( next );
    CHECK( next.TryGet( cube ) );
    CHECK( next.TryGet( paint ) );
    CHECK( next.Drawn() == 0 );
}

TEST_CASE( "An asset of a kind that is not drawn, or one that does not load, has no picture and waits for none" )
{
    Pictures pictures;
    ThumbnailService service( *pictures.mProject, *pictures.mGpu, *pictures.mRenderer );
    const ThumbnailHandle script = service.Request( AssetId::Generate(), "script" );
    const ThumbnailHandle gone = service.Request( AssetId::Generate(), cModelKind );
    pictures.Settle( service );
    CHECK_FALSE( service.TryGet( script ) );
    CHECK_FALSE( service.TryGet( gone ) );
    CHECK( service.Waiting() == 0 );
    CHECK_FALSE( service.TryGet( ThumbnailHandle{} ) );
}
#endif
