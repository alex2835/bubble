
// Light type enum
const int DirLight = 0;
const int PointLight = 1;
const int SpotLight = 2;

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
    float _pad0;

    vec3 color;
    float _pad1;

    vec3 direction;
    float _pad2;

    vec3 position;
    float _pad3;
};

// Uniforms
layout(std140) uniform LightsInfoUniformBuffer
{
    int uNumLights;
    vec3 uViewPos;
};

#define MAX_LIGHTS 128
layout(std140) uniform LightsUniformBuffer
{
    Light uLights[MAX_LIGHTS];
};


// Directional light
vec4 CalcDirLight(Light light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);

    vec3 diffuse = light.color * diff;
    return light.brightness * vec4(diffuse, spec);
}
    
    
// Point light 
vec4 CalcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess);

    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec3 diffuse = light.color * diff * attenuation;
    spec *= attenuation;
    return light.brightness * vec4(diffuse, spec);
}
    
    
// Spot light
vec4 CalcSpotLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0f);

    // specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0f), uMaterial.shininess);

    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0f, 1.0f);

    vec3 diffuse = light.color * diff * attenuation * intensity;
    spec *= attenuation * intensity;
    return light.brightness * vec4(diffuse, spec);
}


vec4 CalcLighting(vec3 normal, vec3 fragPos)
{
    vec4 result = vec4(0.0);
    vec3 viewDir = normalize(uViewPos - fragPos);
    for (int i = 0; i < uNumLights; ++i)
    {
        Light light = uLights[i];
        if (light.type == DirLight)
            result += CalcDirLight(light, normal, viewDir);
        else if (light.type == PointLight)
            result += CalcPointLight(light, normal, fragPos, viewDir);
        else if (light.type == SpotLight)
            result += CalcSpotLight(light, normal, fragPos, viewDir);
    }
    return result;
}