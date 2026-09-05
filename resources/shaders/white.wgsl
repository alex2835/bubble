#include <common>

struct VertexInput
{
    @location(0) aPosition: vec3<f32>,
};

@vertex
fn vs_main( in: VertexInput ) -> @builtin(position) vec4<f32>
{
    return uVertex.uProjection * uVertex.uView * uDraw.uModel * vec4<f32>( in.aPosition, 1.0 );
}

@fragment
fn fs_main() -> @location(0) vec4<f32>
{
    return vec4<f32>( 0.8, 0.8, 0.8, 1.0 );
}
