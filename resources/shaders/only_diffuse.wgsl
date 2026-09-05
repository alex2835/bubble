#include <common>
#include <material>

struct VertexInput
{
    @location(0) aPosition: vec3<f32>,
    @location(1) aNormal: vec3<f32>,
    @location(2) aTexCoords: vec2<f32>,
    @location(3) aTangent: vec3<f32>,
    @location(4) aBitangent: vec3<f32>,
};

@vertex
fn vs_main( in: VertexInput ) -> VertexOutput
{
    var out: VertexOutput;
    out.vFragPos = ( uDraw.uModel * vec4<f32>( in.aPosition, 1.0 ) ).xyz;
    out.vNormal = normalize( uDraw.uNormalMatrix * in.aNormal );
    out.vTexCoords = in.aTexCoords;
    out.vTangent = in.aTangent;
    out.vBitangent = in.aBitangent;
    out.position = uVertex.uProjection * uVertex.uView * vec4<f32>( out.vFragPos, 1.0 );
    return out;
}

@fragment
fn fs_main( in: VertexOutput ) -> @location(0) vec4<f32>
{
    if ( uMaterial.hasDiffuseMap != 0u )
    {
        return textureSample( uDiffuseMap, uSampler, in.vTexCoords );
    }
    return uMaterial.diffuseColor;
}
