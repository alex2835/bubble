
// Standard lit-surface shading, factored out of phong.frag.
//
// A shader that only wants to tint, mask or fade the lit result had to copy the
// whole fragment stage to get at it. With this it is:
//
//     #version 330 core
//     #include <phong>
//     uniform vec4 uColor = vec4(1.0);
//     out vec4 FragColor;
//     void main() { FragColor = PhongFragment(uColor); }
//
// and no vertex stage at all - a shader with only a .frag gets the engine's.
// Pull PhongAlbedo/PhongNormal/PhongShade apart instead when you need to step
// in between.

#include <material>
#include <light>

// Written by the default vertex stage (phong.vert). Declared here so a shader
// using this module does not have to restate them.
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoords;
in mat3 vTBN;


// Base colour before lighting. Falls back to the material's diffuse colour when
// the mesh has no diffuse map - without this an untextured model samples
// whichever texture happened to be bound to unit 0 and comes out black or
// wearing the previous mesh's texture.
vec4 PhongAlbedo()
{
    return uMaterial.hasDiffuseMap
         ? texture(uMaterial.diffuseMap, vTexCoords)
         : uMaterial.diffuseColor;
}


// Geometric normal, or the normal-mapped one when the mesh carries a normal map
// and the material enabled it.
vec3 PhongNormal()
{
    if (!uNormalMapping)
        return normalize(vNormal);

    vec3 normal = texture(uMaterial.normalMap, vTexCoords).rgb;
    normal = normal * 2.0 - 1.0;
    normal.xy *= uNormalMappingStrength;
    return normalize(vTBN * normal);
}


// Lit result for a given base colour. Alpha passes through untouched.
//
// Specular is deliberately not added: it is computed by CalcLighting and
// dropped here, which is what this shader has always done. To turn it on, add
//     lit.rgb += diffuseAndSpecular.a * uMaterial.specularColor.rgb
//                * texture(uMaterial.specularMap, vTexCoords).rgb;
vec4 PhongShade(vec4 albedo)
{
    vec4 diffuseAndSpecular = CalcLighting(PhongNormal(), vFragPos);
    return vec4(diffuseAndSpecular.rgb * albedo.rgb, albedo.a);
}


// The whole fragment stage in one call: sample, drop fully transparent texels,
// shade. `tint` multiplies the base colour; pass vec4(1.0) for plain phong.
vec4 PhongFragment(vec4 tint)
{
    vec4 albedo = PhongAlbedo();
    if (albedo.a < 0.0001)
        discard;

    return PhongShade(albedo * tint);
}
