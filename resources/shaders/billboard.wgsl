#include <common>
#include <material>

struct VertexInput
{
    @location(0) aPosition: vec3<f32>,
    @location(1) aNormal: vec3<f32>,
    @location(2) aTexCoords: vec2<f32>,
};

struct BillboardOutput
{
    @builtin(position) position: vec4<f32>,
    @location(0) vTexCoord: vec2<f32>,
};

@vertex
fn vs_main( in: VertexInput ) -> BillboardOutput
{
    var out: BillboardOutput;
    let worldPos = BillboardPosition( in.aPosition );
    out.position = uVertex.uProjection * uVertex.uView * vec4<f32>( worldPos, 1.0 );
    // Flipped to match the texture convention the quad was authored against.
    out.vTexCoord = vec2<f32>( in.aTexCoords.x, 1.0 - in.aTexCoords.y );
    return out;
}

// The billboard's image arrives as the material's diffuse map: a billboard is a
// quad with exactly one texture, which is what the material bind group already
// describes, so it needs no group of its own.
@fragment
fn fs_main( in: BillboardOutput ) -> @location(0) vec4<f32>
{
    let texColor = textureSample( uDiffuseMap, uSampler, in.vTexCoord );

    // Cut out rather than blend - this is what the GL version did, and the
    // pipeline still has blending disabled.
    if ( texColor.a < 0.01 )
    {
        discard;
    }
    return texColor * uDraw.uTintColor;
}
