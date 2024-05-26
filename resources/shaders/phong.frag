#version 330 core
out vec4 FragColor;

// Material
struct Material
{
    sampler2D diffuse_map;
    sampler2D specular_map;
    sampler2D normal_map;

    vec4 diffuse_color;
    float specular_coef;
    float ambient_coef;
    int shininess;
};

// Light type enum
const int DirLight   = 0;
const int PointLight = 1;
const int SpotLight  = 2;

// Light
struct Light
{
    int type;
    float brightness;

    float constant;
    float linear;
    float quadratic;

    float cutOff;
    float outerCutOff;

    vec3 color;
    float _pad0;
    vec3 direction;
    float _pad1;
    vec3 position;
    float _pad2;
};

layout(std140) uniform LightsInfoUniformBuffer
{
    int uNumLights;
    vec3 uViewPos;
};

#define MAX_LIGHTS 30
layout(std140) uniform Lights
{
    Light lights[MAX_LIGHTS];
};


// Vertex shader output
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoords;
in mat3 vTBN;

// Uniforms
uniform Material uMaterial;
uniform bool uNormalMapping;
uniform float uNormalMappingStrength;

// Function prototypes
vec4 CalcDirLight(Light light, vec3 normal, vec3 view_dir);
vec4 CalcPointLight(Light light, vec3 normal, vec3 frag_pos, vec3 view_dir);
vec4 CalcSpotLight(Light light, vec3 normal, vec3 frag_pos, vec3 view_dir);
    
void main()
{
    // temp
    vec4 tex = texture(uMaterial.diffuse_map, vTexCoords);
    tex.a = 1.0f;
    FragColor = tex;
    return;


    vec4 diffuse_texel = texture(uMaterial.diffuse_map, vTexCoords);
    if (diffuse_texel.a < 0.0001f)
        discard;

    vec4 specular_texel = texture(uMaterial.specular_map, vTexCoords);
    vec3 norm = vNormal;
    if (uNormalMapping)
    {
        norm = texture(uMaterial.normal_map, vTexCoords).rgb;
        norm = normalize(norm * 2.0f - 1.0f);
        norm = normalize(norm * vec3(uNormalMappingStrength, uNormalMappingStrength, 1));
        norm = normalize(vTBN * norm);
    }

    vec3 view_dir = normalize(uViewPos - vFragPos);
    vec4 result = vec4(0.0f);
    vec4 diff_spec = vec4(0.0f);

    //for (int i = 0; i < nLights; i++)
    //{
    //    switch (lights[i].type)
    //    {
    //    case DirLight:
    //        diff_spec += CalcDirLight(lights[i], norm, view_dir);
    //        break;
    //
    //    case PointLight:
    //        diff_spec += CalcPointLight(lights[i], norm, vFragPos, view_dir);
    //        break;
    //
    //    case SpotLight:
    //        diff_spec += CalcSpotLight(lights[i], norm, vFragPos, view_dir);
    //        break;
    //    }
    //}
    vec4  diffuse_light_coef  = max(vec4(uMaterial.ambient_coef), vec4(diff_spec.xyz, 1.0f));
    float specular_light_coef = diff_spec.w;

    result += uMaterial.diffuse_color * diffuse_light_coef * diffuse_texel;
    result += uMaterial.diffuse_color *
              diffuse_light_coef     *
              uMaterial.specular_coef *
              vec4(vec3(specular_light_coef * specular_texel.x), 0);
  
    result = min(result, vec4(1.0f));
    FragColor = result;
}
    
    
    
// Directional light
vec4 CalcDirLight(Light light, vec3 normal, vec3 view_dir)
{
    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 halfwayDir = normalize(lightDir + view_dir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);

    vec3 diffuse = light.color * diff;
    return light.brightness * vec4(diffuse, spec);
}
    
    
// Point light 
vec4 CalcPointLight(Light light, vec3 normal, vec3 frag_pos, vec3 view_dir)
{
    vec3 lightDir = normalize(light.position - frag_pos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 halfwayDir = normalize(lightDir + view_dir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);

    // attenuation
    float distance = length(light.position - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec3 diffuse = light.color * diff * attenuation;
    spec *= attenuation;
    return light.brightness * vec4(diffuse, spec);
}
    
    
// Spot light
vec4 CalcSpotLight(Light light, vec3 normal, vec3 frag_pos, vec3 view_dir)
{
    vec3 lightDir = normalize(light.position - frag_pos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0f);

    // specular shading
    vec3 halfwayDir = normalize(-light.direction + view_dir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0f), uMaterial.shininess);

    // attenuation
    float distance = length(light.position - frag_pos);
    float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0f, 1.0f);

    vec3 diffuse = light.color * diff * attenuation * intensity;
    spec *= attenuation * intensity;
    return light.brightness * vec4(diffuse, spec);
}