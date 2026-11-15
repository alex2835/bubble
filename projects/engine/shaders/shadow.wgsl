// A shadow map: the depth of what the light sees, reversed - nearer the
// light is greater - and nothing else.

struct Object
{
    model: mat4x4f,
    normal: mat4x4f,
}

@group( 0 ) @binding( 0 ) var<uniform> light_view_projection: mat4x4f;
@group( 1 ) @binding( 0 ) var<storage, read> objects: array<Object>;

@vertex
fn vs_main( @builtin( instance_index ) object: u32, @location( 0 ) position: vec3f ) -> @builtin( position ) vec4f
{
    return light_view_projection * objects[object].model * vec4f( position, 1.0 );
}
