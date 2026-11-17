// What is where on screen: each pixel the entity whose surface is nearest,
// for a click to find.

struct Frame
{
    view_projection: mat4x4f,
}

// As lit.wgsl has it: the array's stride is the whole struct.
struct Object
{
    model: mat4x4f,
    normal: mat4x4f,
    tint: vec4f,
    entity: u32,
    skin: u32,
}

@group( 0 ) @binding( 0 ) var<uniform> frame: Frame;
@group( 1 ) @binding( 0 ) var<storage, read> objects: array<Object>;
@group( 1 ) @binding( 1 ) var<storage, read> skins: array<mat4x4f>;

struct Varyings
{
    @builtin( position ) clip: vec4f,
    @location( 0 ) @interpolate( flat ) entity: u32,
}

@vertex
fn vs_main( @builtin( instance_index ) object: u32, @location( 0 ) position: vec3f ) -> Varyings
{
    var out: Varyings;
    out.clip = frame.view_projection * objects[object].model * vec4f( position, 1.0 );
    out.entity = objects[object].entity;
    return out;
}

// As lit.wgsl skins a vertex.
@vertex
fn vs_skinned( @builtin( instance_index ) object: u32,
               @location( 0 ) position: vec3f,
               @location( 3 ) joints: vec4u,
               @location( 4 ) weights: vec4f ) -> Varyings
{
    let first = objects[object].skin;
    let skin = skins[first + joints.x] * weights.x + skins[first + joints.y] * weights.y +
               skins[first + joints.z] * weights.z + skins[first + joints.w] * weights.w;
    var out: Varyings;
    out.clip = frame.view_projection * objects[object].model * skin * vec4f( position, 1.0 );
    out.entity = objects[object].entity;
    return out;
}

@fragment
fn fs_main( in: Varyings ) -> @location( 0 ) u32
{
    return in.entity;
}
