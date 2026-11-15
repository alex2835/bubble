// A model's bytes, as the importer writes them and the loader reads them.
#include "bubble/assets/model_asset.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A model's bytes read back as written, and broken ones say what is wrong" )
{
    ModelAsset model{ .mMin = vec3( -1.0f ), .mMax = vec3( 1.0f ), .mMeshes = {} };
    ModelMesh mesh{ .mMaterial = "brick", .mVertices = {}, .mLods = {} };
    for ( const f32 x : { 0.0f, 1.0f, 0.0f } )
        mesh.mVertices.push_back(
            ModelVertex{ .mPosition = vec3( x, 0, 0 ), .mNormal = vec3( 0, 0, 1 ), .mUv = vec2( x, 0.5f ) } );
    mesh.mLods.push_back( ModelLod{ .mError = 0, .mIndices = { 0, 1, 2 } } );
    mesh.mLods.push_back( ModelLod{ .mError = 0.25f, .mIndices = {} } );
    model.mMeshes.push_back( mesh );

    const string bytes = WriteModelAsset( model );
    const auto readMaybe = TryReadModelAsset( bytes );
    REQUIRE_MESSAGE( readMaybe, readMaybe.error() );
    CHECK( readMaybe->mMax == vec3( 1.0f ) );
    REQUIRE( readMaybe->mMeshes.size() == 1 );
    const ModelMesh& read = readMaybe->mMeshes[0];
    CHECK( read.mMaterial == "brick" );
    CHECK( read.mVertices[1].mUv == vec2( 1.0f, 0.5f ) );
    CHECK( read.mLods[0].mIndices == vector<u32>{ 0, 1, 2 } );
    CHECK( read.mLods[1].mError == 0.25f );

    CHECK( TryReadModelAsset( bytes.substr( 0, bytes.size() - 2 ) ).error() == "a model cut short" );
    CHECK( TryReadModelAsset( bytes + "x" ).error() == "a model with more after its last mesh" );
    CHECK( TryReadModelAsset( "OBJ!" ).error() == "not a model the importer made" );
    model.mMeshes[0].mLods[0].mIndices = { 0, 1, 3 };
    CHECK( TryReadModelAsset( WriteModelAsset( model ) ).error() ==
           "mesh 0 of a model has an index 3 past its 3 vertices" );
}
