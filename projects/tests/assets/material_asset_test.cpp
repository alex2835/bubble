// A material's file: its shader or its base, its flags and its parameters'
// values, read and written as the engine reads it; and what is wrong with
// one, said where.
#include "bubble/assets/material_asset.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

namespace
{
constexpr string_view cRust = "9c41d2a07f3b4e5d8a6b1c2d3e4f5a6b";
constexpr string_view cMetal = "5d2a0b1c2d3e4f5a6b7c8d9e0f1a2b3c";
}

TEST_CASE( "A material's file gives its base, its flags and its parameters, and comes back the same" )
{
    const auto materialMaybe = TryReadMaterialAsset( format( R"({{
        "version": 1,
        "base": {{ "asset": "{}", "path": "materials/metal.mat" }},
        "double_sided": true,
        "params": {{ "base_color": [ 0.8, 0.1, 0.1, 1 ], "roughness": 0.3,
                    "base_color_map": {{ "asset": "{}", "path": "images/rust.png" }} }}
    }})",
                                                             cMetal, cRust ) );
    REQUIRE_MESSAGE( materialMaybe, materialMaybe.error() );
    const MaterialAsset& material = *materialMaybe;
    CHECK( material.mBase == *AssetId::TryParse( cMetal ) );
    CHECK_FALSE( material.mShader );
    CHECK( material.mDoubleSided == true );
    CHECK_FALSE( material.mTransparent );
    REQUIRE( material.mParams.size() == 3 );
    CHECK( material.mParams[0].first == "base_color" );
    CHECK( material.mParams[0].second.mNumbers == vector<f32>{ 0.8f, 0.1f, 0.1f, 1.0f } );
    CHECK( material.mParams[1].second.mNumbers == vector<f32>{ 0.3f } );
    CHECK( material.mParams[2].second.mTexture == *AssetId::TryParse( cRust ) );

    const auto againMaybe = TryReadMaterialAsset( WriteMaterialAsset( material ) );
    REQUIRE( againMaybe );
    CHECK( *againMaybe == material );
}

TEST_CASE( "What is wrong with a material's file is said where it is" )
{
    CHECK( TryReadMaterialAsset( "[]" ).error() == "a material is a JSON object, not an array of 0" );
    CHECK( TryReadMaterialAsset( R"({ "shine": 1 })" ).error() ==
           "1:12: shine: not a field (fields: version, shader, base, double_sided, transparent, params)" );
    CHECK( TryReadMaterialAsset( R"({ "version": 2 })" ).error() == "1:14: version: 2 is not a version this reads (it "
                                                                    "reads 1)" );
    CHECK( TryReadMaterialAsset( R"({ "params": { "roughness": "high" } })" ).error() ==
           "1:28: params.roughness: a number, 1 to 4 numbers or a texture { \"asset\": id }, not \"high\"" );
    CHECK(
        TryReadMaterialAsset( R"({ "params": { "tint": [ 1, 2, 3, 4, 5 ] } })" )
            .error()
            .ends_with( "params.tint: a number, 1 to 4 numbers or a texture { \"asset\": id }, not an array of 5" ) );
    CHECK( TryReadMaterialAsset( R"({ "base": { "asset": "nope" } })" )
               .error()
               .ends_with( "base: an object is not an asset id: 32 hex digits" ) );
    CHECK( TryReadMaterialAsset(
               format( R"({{ "shader": {{ "asset": "{}" }}, "base": {{ "asset": "{}" }} }})", cRust, cMetal ) )
               .error() == "shader: a material with a base takes its base's shader" );
}
