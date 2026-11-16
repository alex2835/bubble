// The debug drawing's lines, each in its own color, no light: over what is
// drawn, behind what is nearer.

struct Frame
{
    view_projection: mat4x4f,
}

@group( 0 ) @binding( 0 ) var<uniform> frame: Frame;

struct Vertex
{
    @location( 0 ) position: vec3f,
    // In linear light.
    @location( 1 ) color: vec4f,
}

struct Varyings
{
    @builtin( position ) clip: vec4f,
    @location( 0 ) color: vec4f,
}

@vertex
fn vs_main( vertex: Vertex ) -> Varyings
{
    var out: Varyings;
    out.clip = frame.view_projection * vec4f( vertex.position, 1.0 );
    out.color = vertex.color;
    return out;
}

@fragment
fn fs_main( in: Varyings ) -> @location( 0 ) vec4f
{
    return in.color;
}
