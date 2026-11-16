// What a material's WGSL declares, as the engine takes it: its parameters,
// their place in the uniform block and their defaults, its textures and
// samplers - and what breaks the contract, said with the file's name.
#include "bubble/render/gpu/shaders.hpp"
#include "bubble/render/material_layout.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>

using namespace bubble;

TEST_CASE( "A material's numbers, textures and samplers come from its WGSL, defaults from its comments" )
{
    const auto layoutMaybe = TryReflectMaterial( R"wgsl(
struct Material
{
    tint: vec4f, // @color @default( 1, 0.5, 0, 1 )
    glow: vec3f, // @color
    speed: f32, // @default( 2.5 )
    offset: vec2f,
}
@group( 2 ) @binding( 0 ) var<uniform> material: Material;
@group( 2 ) @binding( 1 ) var linear: sampler;
@group( 2 ) @binding( 2 ) var bumps: texture_2d<f32>; // @default( normal )
@group( 2 ) @binding( 3 ) var dirt: texture_2d<f32>;

fn surface( in: SurfaceInput ) -> SurfaceOutput { return surface_defaults( in ); }
)wgsl",
                                                 "shaders/water.wgsl" );
    REQUIRE_MESSAGE( layoutMaybe, layoutMaybe.error() );
    const MaterialLayout& layout = *layoutMaybe;
    CHECK( layout.mUniformBinding == 0u );
    CHECK( layout.mUniformSize == 48 );
    CHECK( layout.mSamplers == vector<u32>{ 1 } );

    const MaterialParam& tint = *layout.TryFind( "tint" );
    CHECK( tint.mKind == MaterialParamKind::Number );
    CHECK( tint.mComponents == 4 );
    CHECK( tint.mColor );
    CHECK( tint.mDefault == vector<f32>{ 1.0f, 0.5f, 0.0f, 1.0f } );
    // A color nobody picked is white.
    CHECK( layout.TryFind( "glow" )->mDefault == vector<f32>{ 1.0f, 1.0f, 1.0f } );
    CHECK( layout.TryFind( "glow" )->mOffset == 16 );
    CHECK( layout.TryFind( "speed" )->mOffset == 28 );
    CHECK( layout.TryFind( "speed" )->mDefault == vector<f32>{ 2.5f } );
    CHECK( layout.TryFind( "offset" )->mDefault == vector<f32>{ 0.0f, 0.0f } );
    CHECK_FALSE( layout.TryFind( "offset" )->mColor );

    const MaterialParam& bumps = *layout.TryFind( "bumps" );
    CHECK( bumps.mKind == MaterialParamKind::Texture );
    CHECK( bumps.mBinding == 2 );
    CHECK( bumps.mTextureDefault == TextureDefault::Normal );
    CHECK( layout.TryFind( "dirt" )->mTextureDefault == TextureDefault::White );
    CHECK_FALSE( layout.TryFind( "material" ) );
}

TEST_CASE( "The engine's standard material keeps the contract" )
{
    const auto layoutMaybe = TryReflectMaterial( EngineShader( "standard_material.wgsl" ), "standard_material.wgsl" );
    REQUIRE_MESSAGE( layoutMaybe, layoutMaybe.error() );
    CHECK( layoutMaybe->TryFind( "base_color" )->mColor );
    CHECK( layoutMaybe->TryFind( "roughness" )->mDefault == vector<f32>{ 0.5f } );
    CHECK( layoutMaybe->TryFind( "normal_map" )->mTextureDefault == TextureDefault::Normal );
}

TEST_CASE( "What breaks a material's contract is said with its file's name" )
{
    const auto error = []( string_view source ) { return TryReflectMaterial( source, "shaders/bad.wgsl" ).error(); };
    const string surface = "\nfn surface( in: SurfaceInput ) -> SurfaceOutput { return surface_defaults( in ); }";
    CHECK( error( "fn shade() {}" ) ==
           "shaders/bad.wgsl: no fn surface( in: SurfaceInput ) -> SurfaceOutput, which a material is" );
    CHECK( error( "@group( 0 ) @binding( 0 ) var sky: texture_2d<f32>;" + surface ) ==
           "shaders/bad.wgsl: sky is in group 0; a material binds in group 2 alone, the others are the engine's" );
    CHECK( error( "struct M { count: u32, }\n@group( 2 ) @binding( 0 ) var<uniform> m: M;" + surface ) ==
           "shaders/bad.wgsl: count is a u32; a material's numbers are f32 and vec2f to vec4f" );
    CHECK( error( "struct M { tint: vec4f, // @default( 1, 2 )\n}\n@group( 2 ) @binding( 0 ) var<uniform> m: M;" +
                  surface ) == "shaders/bad.wgsl: tint's @default( 1, 2 ) is not 4 numbers" );
    CHECK( error( "struct M { tint: f32, // @color\n}\n@group( 2 ) @binding( 0 ) var<uniform> m: M;" + surface ) ==
           "shaders/bad.wgsl: tint is a color, which is a vec3f or a vec4f, not a f32" );
    CHECK( error( "@group( 2 ) @binding( 0 ) var dirt: texture_2d<f32>; // @default( grey )" + surface ) ==
           "shaders/bad.wgsl: dirt's @default( grey ) is not a texture's (white, black, normal)" );
    CHECK( error( "@group( 2 ) @binding( 0 ) var depth: texture_depth_2d;" + surface ) ==
           "shaders/bad.wgsl: depth is a texture_depth_2d; a material binds one uniform struct, texture_2d<f32> and "
           "sampler" );
}
