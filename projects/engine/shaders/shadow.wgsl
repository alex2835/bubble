// A shadow map: the depth of what the light sees, reversed - nearer the
// light is greater - and nothing else.

// As lit.wgsl has it: the array's stride is the whole struct.
struct Object
{
    model: mat4x4f,
    normal: mat4x4f,
    tint: vec4f,
    entity: u32,
    skin: u32,
}

@group( 0 ) @binding( 0 ) var<uniform> light_view_projection: mat4x4f;
@group( 1 ) @binding( 0 ) var<storage, read> objects: array<Object>;
@group( 1 ) @binding( 1 ) var<storage, read> skins: array<mat4x4f>;

@vertex
fn vs_main( @builtin( instance_index ) object: u32, @location( 0 ) position: vec3f ) -> @builtin( position ) vec4f
{
    return light_view_projection * objects[object].model * vec4f( position, 1.0 );
}

// As lit.wgsl skins a vertex.
@vertex
fn vs_skinned( @builtin( instance_index ) object: u32,
               @location( 0 ) position: vec3f,
               @location( 3 ) joints: vec4u,
               @location( 4 ) weights: vec4f ) -> @builtin( position ) vec4f
{
    let first = objects[object].skin;
    let skin = skins[first + joints.x] * weights.x + skins[first + joints.y] * weights.y +
               skins[first + joints.z] * weights.z + skins[first + joints.w] * weights.w;
    return light_view_projection * objects[object].model * skin * vec4f( position, 1.0 );
}
