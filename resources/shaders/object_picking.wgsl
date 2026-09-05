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

// The target is R32Uint, so the entity id is written straight out rather than
// being packed into a colour.
@fragment
fn fs_main() -> @location(0) u32
{
    return uDraw.uObjectId;
}
