#version 330 core

#include <material>
//#include <light>

// Fragment input
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoords;
in mat3 vTBN;

// Fragment output
out vec4 FragColor;

    
void main()
{
    // temp
//    vec4 tex = vec4(1.0f);
    vec4 tex = texture(uMaterial.diffuseMap, vTexCoords);
    FragColor = tex;
    return;

//    vec4 diffuse_texel = texture(uMaterial.diffuseMap, vTexCoords);
//    if (diffuse_texel.a < 0.0001f)
//        discard;
//
//    vec4 specular_texel = texture(uMaterial.specularMap, vTexCoords);
//    vec3 norm = vNormal;
//    if (uNormalMapping)
//    {
//        norm = texture(uMaterial.normalMap, vTexCoords).rgb;
//        norm = normalize(norm * 2.0f - 1.0f);
//        norm = normalize(norm * vec3(uNormalMappingStrength, uNormalMappingStrength, 1));
//        norm = normalize(vTBN * norm);
//    }
//
//    vec3 viewDir = normalize(uViewPos - vFragPos);
//    vec4 result = vec4(0.0f);
//    vec4 diff_spec = vec4(0.0f);

    //for (int i = 0; i < nLights; i++)
    //{
    //    switch (lights[i].type)
    //    {
    //    case DirLight:
    //        diff_spec += CalcDirLight(lights[i], norm, viewDir);
    //        break;
    //
    //    case PointLight:
    //        diff_spec += CalcPointLight(lights[i], norm, vFragPos, viewDir);
    //        break;
    //
    //    case SpotLight:
    //        diff_spec += CalcSpotLight(lights[i], norm, vFragPos, viewDir);
    //        break;
    //    }
    //}
//    vec4  diffuse_light_coef  = max(uMaterial.ambientColor, vec4(diff_spec.xyz, 1.0f));
//    float specular_light_coef = diff_spec.w;
//
//    result += uMaterial.diffuseColor * diffuse_light_coef * diffuse_texel;
//    result += uMaterial.diffuseColor *
//              diffuse_light_coef *
//              uMaterial.specularColor *
//              vec4(vec3(specular_light_coef * specular_texel.x), 0);

//    result = min(result, vec4(1.0f));
//    FragColor = result;
}