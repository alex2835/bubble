
// uMaterial comes from <material>, which the including shader pulls in first -
// the include expander has no guards, so this must not include it again.

// Light type enum
const DirLight: i32 = 0;
const PointLight: i32 = 1;
const SpotLight: i32 = 2;

// Padded to an 80 byte stride to match LightUniforms in engine/renderer/light.hpp.
// The explicit _pad fields are what keep the two in step; WGSL's uniform rules
// put vec3 on a 16 byte boundary just as std140 did, and glm::vec3 does not, so
// the C++ side pads by hand and static_asserts every vec3 offset.
struct Light
{
    lightType: i32,
    brightness: f32,

    constant: f32,
    linear: f32,
    quadratic: f32,

    cutOff: f32,
    outerCutOff: f32,
    _pad0: f32,

    color: vec3<f32>,
    _pad1: f32,

    direction: vec3<f32>,
    _pad2: f32,

    position: vec3<f32>,
    _pad3: f32,
};

struct LightsInfo
{
    uNumLights: i32,
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
    uViewPos: vec3<f32>,
    _pad3: f32,
};

// Must match Renderer::cMaxLights in engine/renderer/renderer.hpp.
const MAX_LIGHTS: u32 = 512u;

struct Lights
{
    uLights: array<Light, MAX_LIGHTS>,
};

@group(0) @binding(1) var<uniform> uLightsInfo: LightsInfo;
@group(0) @binding(2) var<uniform> uLights: Lights;


// Directional light
fn CalcDirLight( light: Light, normal: vec3<f32>, viewDir: vec3<f32> ) -> vec4<f32>
{
    let lightDir = normalize( -light.direction );

    // diffuse shading
    let diff = max( dot( normal, lightDir ), 0.0 );

    // specular shading
    let halfwayDir = normalize( lightDir + viewDir );
    let spec = pow( max( dot( normal, halfwayDir ), 0.0 ), f32( uMaterial.shininess ) );

    let diffuse = light.color * diff;
    return light.brightness * vec4<f32>( diffuse, spec );
}


// Point light
fn CalcPointLight( light: Light, normal: vec3<f32>, fragPos: vec3<f32>, viewDir: vec3<f32> ) -> vec4<f32>
{
    let lightDir = normalize( light.position - fragPos );

    let diff = max( dot( normal, lightDir ), 0.0 );

    let halfwayDir = normalize( lightDir + viewDir );
    var spec = pow( max( dot( normal, halfwayDir ), 0.0 ), f32( uMaterial.shininess ) );

    // attenuation
    let dist = length( light.position - fragPos );
    let attenuation = 1.0 / ( light.constant + light.linear * dist + light.quadratic * ( dist * dist ) );

    let diffuse = light.color * diff * attenuation;
    spec = spec * attenuation;
    return light.brightness * vec4<f32>( diffuse, spec );
}


// Spot light
fn CalcSpotLight( light: Light, normal: vec3<f32>, fragPos: vec3<f32>, viewDir: vec3<f32> ) -> vec4<f32>
{
    let lightDir = normalize( light.position - fragPos );

    let diff = max( dot( normal, lightDir ), 0.0 );

    let halfwayDir = normalize( lightDir + viewDir );
    var spec = pow( max( dot( normal, halfwayDir ), 0.0 ), f32( uMaterial.shininess ) );

    let dist = length( light.position - fragPos );
    let attenuation = 1.0 / ( light.constant + light.linear * dist + light.quadratic * ( dist * dist ) );

    // spotlight intensity
    let theta = dot( lightDir, normalize( -light.direction ) );
    let epsilon = light.cutOff - light.outerCutOff;
    let intensity = clamp( ( theta - light.outerCutOff ) / epsilon, 0.0, 1.0 );

    let diffuse = light.color * diff * attenuation * intensity;
    spec = spec * attenuation * intensity;
    return light.brightness * vec4<f32>( diffuse, spec );
}


fn CalcLighting( normal: vec3<f32>, fragPos: vec3<f32> ) -> vec4<f32>
{
    var result = vec4<f32>( 0.0 );
    let viewDir = normalize( uLightsInfo.uViewPos - fragPos );

    for ( var i: i32 = 0; i < uLightsInfo.uNumLights; i = i + 1 )
    {
        let light = uLights.uLights[i];
        if ( light.lightType == DirLight )
        {
            result = result + CalcDirLight( light, normal, viewDir );
        }
        else if ( light.lightType == PointLight )
        {
            result = result + CalcPointLight( light, normal, fragPos, viewDir );
        }
        else if ( light.lightType == SpotLight )
        {
            result = result + CalcSpotLight( light, normal, fragPos, viewDir );
        }
    }
    return result;
}
