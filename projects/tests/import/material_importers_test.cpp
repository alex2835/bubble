// The material and shader importers: a .mat checked and written as the
// engine reads it, a .wgsl kept once it keeps the contract - and what is
// wrong with either, its path first.
#include "bubble/assets/material_asset.hpp"
#include "bubble/import/material_importers.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A material is imported as the engine reads it, and a broken one says its path and place" )
{
    const Importer importer = MaterialImporter();
    CHECK( importer.mExtension == ".mat" );
    CHECK( importer.mKind == cMaterialKind );
    const AssetPath path = *AssetPath::TryFrom( "materials/red.mat" );
    const auto importedMaybe = importer.mImport( R"({ "params": { "roughness": 0.3 } })", Json(), path );
    REQUIRE_MESSAGE( importedMaybe, importedMaybe.error() );
    const auto materialMaybe = TryReadMaterialAsset( *importedMaybe );
    REQUIRE( materialMaybe );
    CHECK( materialMaybe->mParams.front().second.mNumbers == vector<f32>{ 0.3f } );

    CHECK( importer.mImport( R"({ "shine": 1 })", Json(), path ).error() ==
           "materials/red.mat:1:12: shine: not a field (fields: version, shader, base, double_sided, transparent, "
           "params)" );
    CHECK( importer.mImport( "7", Json(), path ).error() == "materials/red.mat: a material is a JSON object, not 7" );
}

TEST_CASE( "A material's WGSL is imported once it keeps the contract" )
{
    const Importer importer = ShaderImporter();
    CHECK( importer.mExtension == ".wgsl" );
    const AssetPath path = *AssetPath::TryFrom( "shaders/glow.wgsl" );
    const string source = "fn surface( in: SurfaceInput ) -> SurfaceOutput { return surface_defaults( in ); }";
    CHECK( importer.mImport( source, Json(), path ) == source );
    CHECK( importer.mImport( "fn shade() {}", Json(), path ).error() ==
           "shaders/glow.wgsl: no fn surface( in: SurfaceInput ) -> SurfaceOutput, which a material is" );
}
