// Models imported as the engine's meshes - assimp reading, meshoptimizer
// ordering and simplifying - and, with a texture, imported and loaded with
// no window: stage 4's "done".
#include "bubble/import/model_importer.hpp"
#ifdef BUBBLE_IMPORT_MODELS
#include "bubble/assets/asset_registry.hpp"
#include "bubble/assets/model_asset.hpp"
#include "bubble/assets/texture_asset.hpp"
#include "bubble/import/project_assets.hpp"
#include "bubble/import/texture_importer.hpp"
#include "bubble/scene/engine_file.hpp"
#include "bubble/types/format.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cCube = R"(o cube
v -1 -1 -1
v  1 -1 -1
v  1  1 -1
v -1  1 -1
v -1 -1  1
v  1 -1  1
v  1  1  1
v -1  1  1
f 1 4 3 2
f 5 6 7 8
f 1 2 6 5
f 2 3 7 6
f 3 4 8 7
f 4 1 5 8
)";

// A bumpy grid of `side` × `side` squares: much for a simplifier to take.
string Grid( u32 side )
{
    string obj;
    for ( u32 y = 0; y <= side; ++y )
        for ( u32 x = 0; x <= side; ++x )
            obj += format( "v {} {} {}\n", x, y, 0.3 * sin( x * 0.7 ) * cos( y * 0.5 ) );
    const auto at = [&]( u32 x, u32 y ) { return y * ( side + 1 ) + x + 1; };
    for ( u32 y = 0; y < side; ++y )
        for ( u32 x = 0; x < side; ++x )
            obj += format( "f {} {} {} {}\n", at( x, y ), at( x + 1, y ), at( x + 1, y + 1 ), at( x, y + 1 ) );
    return obj;
}

expected<ModelAsset, string> TryImport( string_view source, string_view settings = "{}" )
{
    for ( const Importer& importer : ModelImporters() )
        if ( importer.mExtension == ".obj" )
        {
            auto bytesMaybe =
                importer.mImport( source, *Json::TryParse( settings ), *AssetPath::TryFrom( "models/a.obj" ) );
            if ( not bytesMaybe )
                return unexpected( std::move( bytesMaybe.error() ) );
            return TryReadModelAsset( *bytesMaybe );
        }
    return unexpected( "no importer"s );
}
}

TEST_CASE( "An OBJ cube is imported as one mesh of triangles, the box around it" )
{
    const auto modelMaybe = TryImport( cCube );
    REQUIRE_MESSAGE( modelMaybe, modelMaybe.error() );
    CHECK( modelMaybe->mMin == vec3( -1.0f ) );
    CHECK( modelMaybe->mMax == vec3( 1.0f ) );
    REQUIRE( modelMaybe->mMeshes.size() == 1 );
    const ModelMesh& mesh = modelMaybe->mMeshes[0];
    CHECK( mesh.mLods[0].mIndices.size() == 36 );
    CHECK( mesh.mLods[0].mError == 0.0f );
    CHECK( mesh.mVertices.size() >= 8 );
    CHECK( mesh.mVertices.size() <= 36 );
    // Normals made where the file had none.
    CHECK( mesh.mVertices[0].mNormal != vec3( 0.0f ) );

    CHECK( TryImport( cCube, R"({ "scale": 2 })" )->mMax == vec3( 2.0f ) );
}

TEST_CASE( "A mesh is simplified into coarser levels that share its vertices" )
{
    const auto modelMaybe = TryImport( Grid( 32 ), R"({ "lods": [0.5, 0.25, 0.1] })" );
    REQUIRE_MESSAGE( modelMaybe, modelMaybe.error() );
    const ModelMesh& mesh = modelMaybe->mMeshes[0];
    const vector<ModelLod>& lods = mesh.mLods;
    REQUIRE( lods.size() >= 3 );
    CHECK( lods[0].mIndices.size() == 32 * 32 * 6 );
    for ( size_t level = 1; level < lods.size(); ++level )
    {
        CHECK( lods[level].mIndices.size() < lods[level - 1].mIndices.size() );
        CHECK( lods[level].mIndices.size() % 3 == 0 );
        CHECK( lods[level].mError >= lods[level - 1].mError );
        for ( const u32 index : lods[level].mIndices )
            REQUIRE( index < mesh.mVertices.size() );
    }
    CHECK( lods[1].mIndices.size() <= lods[0].mIndices.size() / 2 + 3 );

    // None asked for: the full mesh alone.
    CHECK( TryImport( Grid( 4 ), R"({ "lods": [] })" )->mMeshes[0].mLods.size() == 1 );
}

TEST_CASE( "A model that will not import, and settings that are wrong, say why" )
{
    CHECK( TryImport( cCube, R"({ "lods": [1.5] })" )
               .error()
               .ends_with( "settings.lods: 1.5 is not a share between 0 and 1" ) );
    CHECK( TryImport( cCube, R"({ "flip": true })" )
               .error()
               .ends_with( "settings.flip: not a field (fields: lods, scale)" ) );
    CHECK( TryImport( "v 1 2 3\n" ).error().starts_with( "models/a.obj: " ) );
    vector<string> extensions;
    for ( const Importer& importer : ModelImporters() )
        extensions.push_back( importer.mExtension );
    CHECK( extensions == vector<string>{ ".glb", ".gltf", ".fbx", ".obj" } );
}

TEST_CASE( "A model and a texture are imported and loaded with no window" )
{
    // 2×2 PNG: red, green; blue, white at half alpha.
    constexpr char cPng[] =
        "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x02\x00\x00\x00\x02"
        "\x08\x06\x00\x00\x00\x72\xb6\x0d\x24\x00\x00\x00\x13\x49\x44\x41\x54\x78\x9c\x63\xf8\xcf\xc0\xf0"
        "\x1f\x0c\x81\x34\x08\x34\x00\x00\x49\x49\x09\x78\x28\xa0\xdb\x77\x00\x00\x00\x00\x49\x45\x4e\x44"
        "\xae\x42\x60\x82";
    hmap<string, string> files{ { "models/cube.obj", string( cCube ) },
                                { "textures/brick.png", string( cPng, sizeof cPng - 1 ) } };
    Importers importers;
    for ( vector<Importer> group : { ModelImporters(), TextureImporters() } )
        for ( Importer& importer : group )
            importers.Add( std::move( importer ) );
    ProjectAssets project( ProjectFiles::InMemory( files ), std::move( importers ), cEngineFileExtensions );
    const auto refreshMaybe = project.TryRefresh();
    REQUIRE_MESSAGE( refreshMaybe, refreshMaybe.error() );
    CHECK( refreshMaybe->mErrors.empty() );
    CHECK( refreshMaybe->mImported.size() == 2 );

    Jobs jobs( 2 );
    AssetRegistry assets(
        project.GetCatalog(),
        [&]( const AssetId& assetId, string_view kind ) {
            return project.GetFiles().TryRead( ImportedPath( assetId, kind ) );
        },
        jobs );
    RegisterModelLoader( assets );
    RegisterTextureLoader( assets );
    // Loaded as the game loads: on the pool, ready at the main thread's turn.
    const auto cube = assets.Load<ModelAsset>( *AssetPath::TryFrom( "models/cube.obj" ) );
    const auto brick = assets.Load<TextureAsset>( *AssetPath::TryFrom( "textures/brick.png" ) );
    for ( u32 i = 0; i < 5000 and ( cube.State() == AssetState::Loading or brick.State() == AssetState::Loading ); ++i )
    {
        jobs.RunMainQueue();
        this_thread::sleep_for( chrono::milliseconds( 1 ) );
    }
    REQUIRE_MESSAGE( cube.Ready(), cube.Entry().Error() );
    REQUIRE_MESSAGE( brick.Ready(), brick.Entry().Error() );
    CHECK( cube.TryGet()->mMeshes[0].mLods[0].mIndices.size() == 36 );
    CHECK( brick.TryGet()->mMips[0].mWidth == 2 );
}
#endif
