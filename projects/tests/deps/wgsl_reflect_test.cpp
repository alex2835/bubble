// wgsl_reflect: structs laid out as the uniform address space lays them out,
// bindings, functions, and the comments callers annotate with. A wrong offset
// does not fail loudly - it draws garbage - so layouts are pinned against
// values worked out by hand from WGSL's rules.
#include "bubble/types/containers.hpp"
#include "bubble/types/stream.hpp"
#include <doctest.h>
#include <wgsl_reflect/wgsl_reflect.hpp>

using namespace bubble;

TEST_CASE( "Scalars and vectors sit at their alignment" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct S {
    a: f32,
    b: vec3<f32>,
    c: f32,
    d: vec2<f32>,
    e: vec4f,
}
)" );
    const auto* s = module.FindStruct( "S" );
    REQUIRE( s != nullptr );
    // A vec3 aligns to 16 and takes 12, so a scalar fits after it.
    CHECK( s->FindMember( "a" )->mOffset == 0 );
    CHECK( s->FindMember( "b" )->mOffset == 16 );
    CHECK( s->FindMember( "b" )->mSize == 12 );
    CHECK( s->FindMember( "c" )->mOffset == 28 );
    CHECK( s->FindMember( "d" )->mOffset == 32 );
    CHECK( s->FindMember( "e" )->mOffset == 48 );
    CHECK( s->mSize == 64 );
    CHECK( s->mAlign == 16 );
}

TEST_CASE( "A matrix is columns of padded vectors" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct M {
    m3: mat3x3<f32>,
    m4: mat4x4<f32>,
    m2: mat2x2<f32>,
}
)" );
    const auto* s = module.FindStruct( "M" );
    REQUIRE( s != nullptr );
    CHECK( s->FindMember( "m3" )->mSize == 48 );
    CHECK( s->FindMember( "m4" )->mOffset == 48 );
    CHECK( s->FindMember( "m4" )->mSize == 64 );
    CHECK( s->FindMember( "m2" )->mOffset == 112 );
    CHECK( s->FindMember( "m2" )->mSize == 16 );
}

TEST_CASE( "A struct inside a struct and an array of them take the inner one's layout" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct Light {
    position: vec3f,
    range: f32,
    color: vec3f,
}
struct Lights {
    count: u32,
    all: array<Light, 4>,
}
)" );
    const auto* light = module.FindStruct( "Light" );
    REQUIRE( light != nullptr );
    CHECK( light->mSize == 32 );
    const auto* lights = module.FindStruct( "Lights" );
    REQUIRE( lights != nullptr );
    CHECK( lights->FindMember( "all" )->mOffset == 16 );
    CHECK( lights->mSize == 16 + 32 * 4 );
}

TEST_CASE( "Bindings are the resources in groups, a private var is not one" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct Material { color: vec4<f32>, }
@group(1) @binding(0) var<uniform> material: Material;
@group(1) @binding(1) var albedo: texture_2d<f32>; // @default( black )
@group(1) @binding(4) var linear: sampler;
@group(2) @binding(0) var<uniform> draw: Material;

var<private> somethingPrivate: f32;

fn helper() -> f32 {
    // A brace inside a body does not confuse the scan.
    if (true) { return 1.0; }
    return 0.0;
}

fn surface() {}
)" );
    CHECK( module.mErrors.empty() );
    CHECK( module.mBindings.size() == 4 );
    const auto* material = module.FindBinding( "material" );
    REQUIRE( material != nullptr );
    CHECK( material->mGroup == 1 );
    CHECK( material->mBinding == 0 );
    CHECK( material->mAddressSpace == wgsl_reflect::AddressSpace::Uniform );
    CHECK( material->mType.mKind == wgsl_reflect::TypeKind::Struct );
    const auto* albedo = module.FindBinding( "albedo" );
    REQUIRE( albedo != nullptr );
    CHECK( albedo->mType.mKind == wgsl_reflect::TypeKind::Texture );
    CHECK( albedo->mTrailingComment == "@default( black )" );
    CHECK( module.FindBinding( "linear" )->mType.mKind == wgsl_reflect::TypeKind::Sampler );
    CHECK( module.FindBinding( "somethingPrivate" ) == nullptr );
    CHECK( module.FindBindingAt( 2, 0 )->mName == "draw" );
    CHECK( module.mFunctions == vector<string>{ "helper", "surface" } );
}

TEST_CASE( "@align and @size move and widen a member" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct A {
    a: f32,
    @align(16) b: f32,
    @size(32) c: f32,
    d: f32,
}
)" );
    const auto* s = module.FindStruct( "A" );
    REQUIRE( s != nullptr );
    CHECK( s->FindMember( "b" )->mOffset == 16 );
    CHECK( s->FindMember( "c" )->mSize == 32 );
    CHECK( s->FindMember( "d" )->mOffset == 52 );
}

TEST_CASE( "The comment on a member's line is kept for the caller's annotations" )
{
    const auto module = wgsl_reflect::Parse( R"(
struct Material {
    color: vec4<f32>,     // @default(1, 1, 1, 1) @color
    strength: f32,        // @default(0.5)
    /* a block comment does not
       break the member that follows */
    count: i32,
}
)" );
    const auto* s = module.FindStruct( "Material" );
    REQUIRE( s != nullptr );
    CHECK( s->mMembers.size() == 3 );
    CHECK( s->FindMember( "color" )->mTrailingComment == "@default(1, 1, 1, 1) @color" );
    CHECK( s->FindMember( "strength" )->mTrailingComment == "@default(0.5)" );
    CHECK( s->FindMember( "count" )->mOffset == 20 );
}

TEST_CASE( "A bool cannot be in a buffer" )
{
    const auto module = wgsl_reflect::Parse( "struct B { flag: bool, }" );
    const auto* s = module.FindStruct( "B" );
    REQUIRE( s != nullptr );
    CHECK_FALSE( s->FindMember( "flag" )->mType.IsHostShareable() );
}
