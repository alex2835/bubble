#include <common>

struct VertexInput
{
    @location(0) aPosition: vec3<f32>,
};

@vertex
fn vs_main( in: VertexInput ) -> @builtin(position) vec4<f32>
{
    let worldPos = BillboardPosition( in.aPosition );
    return uVertex.uProjection * uVertex.uView * vec4<f32>( worldPos, 1.0 );
}

@fragment
fn fs_main() -> @location(0) u32
{
    return uDraw.uObjectId;
}
