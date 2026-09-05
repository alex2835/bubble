
// Standard lit-surface shading, factored out of phong.wgsl.
//
// A shader that only wants to tint, mask or fade the lit result would otherwise
// have to copy the whole fragment stage to get at it. With this it is:
//
//     #include <phong>
//     @fragment
//     fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
//         return PhongFragment(in);
//     }
//
// Pull PhongAlbedo/PhongNormal/PhongShade apart instead when you need to step
// in between.
//
// Unlike the GLSL version this cannot declare the varyings itself - WGSL has no
// free-floating `in` declarations - so they arrive as a VertexOutput, which
// <common> defines.

#include <material>
#include <light>


// Base colour before lighting. Falls back to the material's diffuse colour when
// the mesh has no diffuse map - without this an untextured model samples the
// engine's 1x1 white placeholder and loses its colour entirely.
fn PhongAlbedo( in: VertexOutput ) -> vec4<f32>
{
    if ( uMaterial.hasDiffuseMap != 0u )
    {
        return textureSample( uDiffuseMap, uSampler, in.vTexCoords );
    }
    return uMaterial.diffuseColor;
}


// Geometric normal, or the normal-mapped one when the mesh carries a normal map
// and the material enabled it.
fn PhongNormal( in: VertexOutput ) -> vec3<f32>
{
    if ( uMaterial.normalMapping == 0u )
    {
        return normalize( in.vNormal );
    }

    var normal = textureSample( uNormalMap, uSampler, in.vTexCoords ).rgb;
    normal = normal * 2.0 - 1.0;
    normal = vec3<f32>( normal.xy * uMaterial.normalMappingStrength, normal.z );

    // The TBN basis, rebuilt from the three varyings. GLSL passed it as a mat3
    // varying; WGSL only interpolates scalars and vectors.
    let tbn = mat3x3<f32>( normalize( in.vTangent ),
                           normalize( in.vBitangent ),
                           normalize( in.vNormal ) );
    return normalize( tbn * normal );
}


// Lit result for a given base colour. Alpha passes through untouched.
//
// Specular is deliberately not added: it is computed by CalcLighting and
// dropped here, which is what this shader has always done. To turn it on, add
//     lit.rgb += diffuseAndSpecular.a * uMaterial.specularColor.rgb
//                * textureSample(uSpecularMap, uSampler, in.vTexCoords).rgb;
fn PhongShade( in: VertexOutput, albedo: vec4<f32> ) -> vec4<f32>
{
    let diffuseAndSpecular = CalcLighting( PhongNormal( in ), in.vFragPos );
    return vec4<f32>( diffuseAndSpecular.rgb * albedo.rgb, albedo.a );
}


// The whole fragment stage in one call: sample, drop fully transparent texels,
// shade.
fn PhongFragment( in: VertexOutput ) -> vec4<f32>
{
    let albedo = PhongAlbedo( in );
    if ( albedo.a < 0.0001 )
    {
        discard;
    }
    return PhongShade( in, albedo );
}


// The vertex stage nearly every lit shader wants. Named rather than being
// `vs_main` so a shader can still write its own.
fn PhongVertex( aPosition: vec3<f32>,
                aNormal: vec3<f32>,
                aTexCoords: vec2<f32>,
                aTangent: vec3<f32>,
                aBitangent: vec3<f32> ) -> VertexOutput
{
    var out: VertexOutput;
    out.vFragPos = ( uDraw.uModel * vec4<f32>( aPosition, 1.0 ) ).xyz;
    out.vNormal = normalize( uDraw.uNormalMatrix * aNormal );
    out.vTexCoords = aTexCoords;

    // Left exactly as it was: swapping uModel for uNormalMatrix here would
    // change the basis under non-uniform scale.
    out.vTangent   = normalize( ( uDraw.uModel * vec4<f32>( aTangent, 0.0 ) ).xyz );
    out.vBitangent = normalize( ( uDraw.uModel * vec4<f32>( aBitangent, 0.0 ) ).xyz );

    out.position = uVertex.uProjection * uVertex.uView * vec4<f32>( out.vFragPos, 1.0 );
    return out;
}
