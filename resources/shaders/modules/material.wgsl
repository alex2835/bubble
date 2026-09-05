
// Material - group 1.
//
// The three sampler2D fields that used to sit inside the GLSL Material struct
// are separate bindings now: WGSL does not allow a texture or a sampler inside
// a struct. The has*Map flags stayed, and still drive the same branches.
//
// A material with no map of a given kind still fills that binding - the engine
// binds a 1x1 white texture - because a bind group has to match its layout
// exactly. One layout and one pipeline therefore serve every material.

struct Material
{
    diffuseColor: vec4<f32>,
    specularColor: vec4<f32>,
    ambientColor: vec4<f32>,
    hasDiffuseMap: u32,
    hasSpecularMap: u32,
    hasNormalMap: u32,
    shininess: i32,
    shininessStrength: f32,
    normalMapping: u32,
    normalMappingStrength: f32,
    _pad0: f32,
};

@group(1) @binding(0) var<uniform> uMaterial: Material;
@group(1) @binding(1) var uDiffuseMap: texture_2d<f32>;
@group(1) @binding(2) var uSpecularMap: texture_2d<f32>;
@group(1) @binding(3) var uNormalMap: texture_2d<f32>;
@group(1) @binding(4) var uSampler: sampler;
