// The standard material: PBR metallic-roughness as glTF has it. A color and
// its map, metallic and roughness and their map (blue and green), a normal
// map, ambient occlusion (red), emission and its map. The maps are images
// imported as sRGB for colors - the color and emission maps - and as data
// for the rest.

struct Material
{
    base_color: vec4f, // @color @default( 1, 1, 1, 1 )
    emissive: vec3f, // @color @default( 0, 0, 0 )
    metallic: f32, // @default( 0 )
    roughness: f32, // @default( 0.5 )
    normal_scale: f32, // @default( 1 )
    occlusion_strength: f32, // @default( 1 )
    emissive_strength: f32, // @default( 1 )
}

@group( 2 ) @binding( 0 ) var<uniform> material: Material;
@group( 2 ) @binding( 1 ) var material_sampler: sampler;
@group( 2 ) @binding( 2 ) var base_color_map: texture_2d<f32>;
@group( 2 ) @binding( 3 ) var metallic_roughness_map: texture_2d<f32>;
@group( 2 ) @binding( 4 ) var normal_map: texture_2d<f32>; // @default( normal )
@group( 2 ) @binding( 5 ) var occlusion_map: texture_2d<f32>;
@group( 2 ) @binding( 6 ) var emissive_map: texture_2d<f32>;

fn surface( in: SurfaceInput ) -> SurfaceOutput
{
    var out = surface_defaults( in );
    let base = textureSample( base_color_map, material_sampler, in.uv ) * material.base_color;
    out.albedo = base.rgb;
    out.alpha = base.a;
    let metallic_roughness = textureSample( metallic_roughness_map, material_sampler, in.uv );
    out.metallic = material.metallic * metallic_roughness.b;
    out.roughness = material.roughness * metallic_roughness.g;
    let tangent_normal = textureSample( normal_map, material_sampler, in.uv ).xyz * 2.0 - 1.0;
    out.normal = perturb_normal( in, vec3f( tangent_normal.xy * material.normal_scale, tangent_normal.z ) );
    let occlusion = textureSample( occlusion_map, material_sampler, in.uv ).r;
    out.occlusion = mix( 1.0, occlusion, material.occlusion_strength );
    out.emissive = material.emissive * material.emissive_strength *
                   textureSample( emissive_map, material_sampler, in.uv ).rgb;
    return out;
}
