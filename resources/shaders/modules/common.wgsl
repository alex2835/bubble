// Bindings the engine provides to every shader.
//
// The group indices match BindGroupIndex in engine/renderer/pipeline.hpp, and
// the struct layouts match the C++ ones byte for byte. Under OpenGL most of
// this was loose uniforms set by name; WebGPU has none, so everything a shader
// reads has to arrive through one of these three groups.
//
// A shader may leave any of them unused - the pipeline layout declares all
// three regardless, which is what lets one frame bind group serve every shader.

// ---------------------------------------------------------------------------
// group 0 - per pass
// ---------------------------------------------------------------------------

struct VertexUniforms
{
    uProjection: mat4x4<f32>,
    uView: mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uVertex: VertexUniforms;


// ---------------------------------------------------------------------------
// group 2 - per draw, reached through a dynamic offset
// ---------------------------------------------------------------------------

struct DrawUniforms
{
    uModel: mat4x4<f32>,
    // transpose(inverse(uModel)), supplied by the renderer. It is the same for
    // every vertex of a draw, and computing a 4x4 inverse per vertex is a lot
    // of work to arrive at one value.
    uNormalMatrix: mat3x3<f32>,
    uBillboardPos: vec4<f32>,
    uBillboardSize: vec4<f32>,
    uTintColor: vec4<f32>,
    uObjectId: u32,
    _pad0: u32,
    _pad1: u32,
    _pad2: u32,
};
@group(2) @binding(0) var<uniform> uDraw: DrawUniforms;


// What the vertex stage hands the fragment stage. WGSL has no free-floating
// in/out declarations, so the varyings the GLSL modules declared separately are
// one struct here.
struct VertexOutput
{
    @builtin(position) position: vec4<f32>,
    @location(0) vFragPos: vec3<f32>,
    @location(1) vNormal: vec3<f32>,
    @location(2) vTexCoords: vec2<f32>,
    @location(3) vTangent: vec3<f32>,
    @location(4) vBitangent: vec3<f32>,
};


// The billboard corner in world space, from the camera's right and up axes.
fn BillboardPosition( localPos: vec3<f32> ) -> vec3<f32>
{
    let cameraRight = vec3<f32>( uVertex.uView[0][0], uVertex.uView[1][0], uVertex.uView[2][0] );
    let cameraUp    = vec3<f32>( uVertex.uView[0][1], uVertex.uView[1][1], uVertex.uView[2][1] );

    return uDraw.uBillboardPos.xyz
         + cameraRight * localPos.x * uDraw.uBillboardSize.x
         + cameraUp    * localPos.y * uDraw.uBillboardSize.y;
}
