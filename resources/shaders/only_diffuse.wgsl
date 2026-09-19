use common;
use material;

struct VertexInput
{
    @location(0) aPosition: vec3<f32>,
    @location(1) aNormal: vec3<f32>,
    @location(2) aTexCoords: vec2<f32>,
    @location(3) aTangent: vec3<f32>,
    @location(4) aBitangent: vec3<f32>,
};

fn Vertex( position: vec3<f32>, normal: vec3<f32>, texCoords: vec2<f32>,
           tangent: vec3<f32>, bitangent: vec3<f32> ) -> VertexOutput
{
    var out: VertexOutput;
    out.vFragPos = ( uDraw.uModel * vec4<f32>( position, 1.0 ) ).xyz;
    out.vNormal = normalize( uDraw.uNormalMatrix * normal );
    out.vTexCoords = texCoords;
    out.vTangent = tangent;
    out.vBitangent = bitangent;
    out.position = uVertex.uProjection * uVertex.uView * vec4<f32>( out.vFragPos, 1.0 );
    return out;
}

@vertex
fn vs_main( in: VertexInput ) -> VertexOutput
{
    return Vertex( in.aPosition, in.aNormal, in.aTexCoords, in.aTangent, in.aBitangent );
}

@vertex
fn vs_skinned( in: VertexInput, skin: SkinInput ) -> VertexOutput
{
    let m = SkinMatrix( skin.aJoints, skin.aWeights );
    let r = mat3x3<f32>( m[0].xyz, m[1].xyz, m[2].xyz );
    return Vertex( ( m * vec4<f32>( in.aPosition, 1.0 ) ).xyz, normalize( r * in.aNormal ),
                   in.aTexCoords, normalize( r * in.aTangent ), normalize( r * in.aBitangent ) );
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
