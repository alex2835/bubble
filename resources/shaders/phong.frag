#version 330 core

#include <material>
#include <light>

// Fragment input
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in mat3 vTBN;

// Fragment output
out vec4 FragColor;

    
void main()
{
    vec4 diffuse_texel = texture(uMaterial.diffuseMap, vTexCoord);
    if (diffuse_texel.a < 0.0001f)
        discard;
        
    vec4 specular_texel = texture(uMaterial.specularMap, vTexCoord);
    
    vec3 norm = vNormal;
    if (uNormalMapping)
    {
        norm = texture(uMaterial.normalMap, vTexCoord).rgb;
        norm = norm * 2.0f - 1.0f;
        norm.xy *= uNormalMappingStrength;
        norm = normalize(vTBN * norm);
    }
    
    vec4 diff_spec = CalcLighting(norm, vFragPos);
    vec3 diffuse_light = diff_spec.rgb;
    float specular_light = diff_spec.a;
    
    // Diffuse: ambient + light contribution
    vec3 diffuse = max(uMaterial.ambientColor.rgb, diffuse_light) * diffuse_texel.rgb;
    
    // Specular: light contribution only (no ambient)
    vec3 specular = specular_light * uMaterial.specularColor.rgb * specular_texel.rgb;
    
    vec3 result = diffuse + specular;
    
    FragColor = vec4(result, diffuse_texel.a);
}