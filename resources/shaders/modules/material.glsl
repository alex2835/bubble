
// Material
struct Material
{
    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;

    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;

    vec4 diffuseColor;
    vec4 specularColor;
    vec4 ambientColor;
    int shininess;
    float shininessStrength;
};

// Uniforms
uniform Material uMaterial;
uniform bool uNormalMapping;
uniform float uNormalMappingStrength;
