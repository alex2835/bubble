// Self test for wgsl_reflect.
//
// The layouts checked here are the ones the engine actually depends on, taken
// from resources/shaders/modules. A wrong offset does not fail loudly - it
// renders as garbage - so they are pinned against hand-computed values from the
// WGSL uniform address space rules rather than against whatever the parser
// happens to produce.

#include "wgsl_reflect/wgsl_reflect.hpp"

#include <iostream>
#include <string>

namespace
{

int gFailures = 0;

void Check( bool condition, const std::string& what )
{
    if ( condition )
        return;
    std::cout << "FAIL: " << what << "\n";
    gFailures++;
}

template <typename T>
void CheckEq( const T& actual, const T& expected, const std::string& what )
{
    if ( actual == expected )
        return;
    std::cout << "FAIL: " << what << " - got " << actual << ", expected " << expected << "\n";
    gFailures++;
}

void TestScalarAndVectorLayout()
{
    const char* source = R"(
struct S {
    a: f32,
    b: vec3<f32>,
    c: f32,
    d: vec2<f32>,
    e: vec4<f32>,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "S" );
    Check( s != nullptr, "struct S is found" );
    if ( not s )
        return;

    // a at 0 (size 4). b is a vec3, which aligns to 16, so it starts at 16 and
    // occupies 12. c then fits at 28. d is a vec2, aligned to 8, so 32. e is a
    // vec4, aligned to 16, so 48.
    CheckEq( s->FindMember( "a" )->mOffset, 0u, "S.a offset" );
    CheckEq( s->FindMember( "b" )->mOffset, 16u, "S.b offset" );
    CheckEq( s->FindMember( "b" )->mSize, 12u, "S.b size" );
    CheckEq( s->FindMember( "c" )->mOffset, 28u, "S.c offset" );
    CheckEq( s->FindMember( "d" )->mOffset, 32u, "S.d offset" );
    CheckEq( s->FindMember( "e" )->mOffset, 48u, "S.e offset" );
    CheckEq( s->mSize, 64u, "S size" );
    CheckEq( s->mAlign, 16u, "S align" );
}

void TestMatrixLayout()
{
    const char* source = R"(
struct M {
    m3: mat3x3<f32>,
    m4: mat4x4<f32>,
    m2: mat2x2<f32>,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "M" );
    Check( s != nullptr, "struct M is found" );
    if ( not s )
        return;

    // A matrix is columns of padded vectors. mat3x3 is three vec3 columns each
    // rounded up to 16, so 48 - not 36. This is the one the engine got wrong
    // under std140, where the size function returned the mat4 value.
    CheckEq( s->FindMember( "m3" )->mSize, 48u, "mat3x3 size" );
    CheckEq( s->FindMember( "m3" )->mOffset, 0u, "mat3x3 offset" );
    CheckEq( s->FindMember( "m4" )->mSize, 64u, "mat4x4 size" );
    CheckEq( s->FindMember( "m4" )->mOffset, 48u, "mat4x4 offset" );
    CheckEq( s->FindMember( "m2" )->mSize, 16u, "mat2x2 size" );
    CheckEq( s->FindMember( "m2" )->mOffset, 112u, "mat2x2 offset" );
}

void TestEngineDrawUniforms()
{
    // Must match DrawUniforms in engine/renderer/pipeline.hpp, which the C++
    // side static_asserts at 176 bytes.
    const char* source = R"(
struct DrawUniforms {
    uModel: mat4x4<f32>,
    uNormalMatrix: mat3x3<f32>,
    uBillboardPos: vec4<f32>,
    uBillboardSize: vec4<f32>,
    uTintColor: vec4<f32>,
    uObjectId: u32,
    _pad0: u32,
    _pad1: u32,
    _pad2: u32,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "DrawUniforms" );
    Check( s != nullptr, "DrawUniforms is found" );
    if ( not s )
        return;

    CheckEq( s->FindMember( "uModel" )->mOffset, 0u, "uModel offset" );
    CheckEq( s->FindMember( "uNormalMatrix" )->mOffset, 64u, "uNormalMatrix offset" );
    CheckEq( s->FindMember( "uBillboardPos" )->mOffset, 112u, "uBillboardPos offset" );
    CheckEq( s->FindMember( "uBillboardSize" )->mOffset, 128u, "uBillboardSize offset" );
    CheckEq( s->FindMember( "uTintColor" )->mOffset, 144u, "uTintColor offset" );
    CheckEq( s->FindMember( "uObjectId" )->mOffset, 160u, "uObjectId offset" );
    CheckEq( s->mSize, 176u, "DrawUniforms size" );
}

void TestEngineLightLayout()
{
    // Must match the 80 byte stride the renderer computes for its light array.
    const char* source = R"(
struct Light {
    lightType: i32,
    brightness: f32,
    constant: f32,
    linear: f32,
    quadratic: f32,
    cutOff: f32,
    outerCutOff: f32,
    _pad0: f32,
    color: vec3<f32>,
    _pad1: f32,
    direction: vec3<f32>,
    _pad2: f32,
    position: vec3<f32>,
    _pad3: f32,
}
struct Lights {
    uLights: array<Light, 128>,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* light = module.FindStruct( "Light" );
    Check( light != nullptr, "Light is found" );
    if ( not light )
        return;

    CheckEq( light->mSize, 80u, "Light stride" );
    CheckEq( light->FindMember( "color" )->mOffset, 32u, "Light.color offset" );
    CheckEq( light->FindMember( "direction" )->mOffset, 48u, "Light.direction offset" );
    CheckEq( light->FindMember( "position" )->mOffset, 64u, "Light.position offset" );

    const auto* lights = module.FindStruct( "Lights" );
    Check( lights != nullptr, "Lights is found" );
    if ( lights )
        CheckEq( lights->mSize, 80u * 128u, "Lights array size" );
}

void TestBindings()
{
    const char* source = R"(
struct Material { diffuseColor: vec4<f32>, }
@group(1) @binding(0) var<uniform> uMaterial: Material;
@group(1) @binding(1) var uDiffuseMap: texture_2d<f32>;
@group(1) @binding(4) var uSampler: sampler;
@group(2) @binding(0) var<uniform> uDraw: Material;

// Not a bind group resource, and must not be reported as one.
var<private> somethingPrivate: f32;

fn helper() -> f32 {
    // A brace inside a function body must not confuse the scanner.
    if (true) { return 1.0; }
    return 0.0;
}
)";
    const auto module = wgsl_reflect::Parse( source );

    CheckEq( module.mBindings.size(), size_t( 4 ), "binding count" );

    const auto* material = module.FindBinding( "uMaterial" );
    Check( material != nullptr, "uMaterial binding found" );
    if ( material )
    {
        CheckEq( material->mGroup, 1u, "uMaterial group" );
        CheckEq( material->mBinding, 0u, "uMaterial binding index" );
        Check( material->mAddressSpace == wgsl_reflect::AddressSpace::Uniform,
               "uMaterial is a uniform" );
        Check( material->mType.mKind == wgsl_reflect::TypeKind::Struct,
               "uMaterial is a struct" );
    }

    const auto* diffuse = module.FindBinding( "uDiffuseMap" );
    Check( diffuse != nullptr and diffuse->mType.mKind == wgsl_reflect::TypeKind::Texture,
           "uDiffuseMap is a texture" );

    const auto* sampler = module.FindBinding( "uSampler" );
    Check( sampler != nullptr and sampler->mType.mKind == wgsl_reflect::TypeKind::Sampler,
           "uSampler is a sampler" );

    Check( module.FindBinding( "somethingPrivate" ) == nullptr,
           "a private var is not a binding" );

    const auto* atDraw = module.FindBindingAt( 2, 0 );
    Check( atDraw != nullptr and atDraw->mName == "uDraw", "lookup by group and binding" );
}

void TestExplicitAlignAndSize()
{
    const char* source = R"(
struct A {
    a: f32,
    @align(16) b: f32,
    @size(32) c: f32,
    d: f32,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "A" );
    Check( s != nullptr, "struct A is found" );
    if ( not s )
        return;

    CheckEq( s->FindMember( "b" )->mOffset, 16u, "@align(16) moves the member" );
    CheckEq( s->FindMember( "c" )->mSize, 32u, "@size(32) widens the member" );
    CheckEq( s->FindMember( "d" )->mOffset, 52u, "@size affects what follows" );
}

void TestCommentsAndAnnotations()
{
    const char* source = R"(
struct UserUniforms {
    uColor: vec4<f32>,     // @default(1, 1, 1, 1)
    uStrength: f32,        // @default(0.5)
    /* a block comment must not
       break the member that follows */
    uCount: i32,
}
)";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "UserUniforms" );
    Check( s != nullptr, "UserUniforms is found" );
    if ( not s )
        return;

    CheckEq( s->mMembers.size(), size_t( 3 ), "member count with comments" );
    CheckEq( s->FindMember( "uColor" )->mTrailingComment,
             std::string( "@default(1, 1, 1, 1)" ), "trailing comment captured" );
    CheckEq( s->FindMember( "uStrength" )->mTrailingComment,
             std::string( "@default(0.5)" ), "trailing comment on a scalar" );
    CheckEq( s->FindMember( "uCount" )->mOffset, 20u, "member after a block comment" );
}

void TestBoolIsNotHostShareable()
{
    const char* source = "struct B { flag: bool, }";
    const auto module = wgsl_reflect::Parse( source );
    const auto* s = module.FindStruct( "B" );
    Check( s != nullptr, "struct B is found" );
    if ( s )
        Check( not s->FindMember( "flag" )->mType.IsHostShareable(),
               "bool is rejected as buffer content" );
}

}

int main()
{
    TestScalarAndVectorLayout();
    TestMatrixLayout();
    TestEngineDrawUniforms();
    TestEngineLightLayout();
    TestBindings();
    TestExplicitAlignAndSize();
    TestCommentsAndAnnotations();
    TestBoolIsNotHostShareable();

    if ( gFailures == 0 )
    {
        std::cout << "all wgsl_reflect tests passed\n";
        return 0;
    }
    std::cout << gFailures << " wgsl_reflect test(s) failed\n";
    return 1;
}
