#include "engine/pch/pch.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
struct AttenuationData
{
    f32 distance; // Distance in meters
    f32 linear;
    f32 quadratic;
};

constexpr AttenuationData AttenuationLookup[] = {
    { 7.0f,    0.7f,    1.8f      },
    { 13.0f,   0.35f,   0.44f     },
    { 20.0f,   0.22f,   0.20f     },
    { 32.0f,   0.14f,   0.07f     },
    { 50.0f,   0.09f,   0.032f    },
    { 65.0f,   0.07f,   0.017f    },
    { 100.0f,  0.045f,  0.0075f   },
    { 160.0f,  0.027f,  0.0028f   },
    { 200.0f,  0.022f,  0.0019f   },
    { 325.0f,  0.014f,  0.0007f   },
    { 600.0f,  0.007f,  0.0002f   },
    { 3250.0f, 0.0014f, 0.000007f },
};

// Takes distance in meters (7m to 3250m)
// Returns { linear, quadratic } attenuation constants
std::pair<f32, f32> GetAttenuationConstans( f32 distanceMeters )
{
    constexpr size_t tableSize = sizeof(AttenuationLookup) / sizeof(AttenuationLookup[0]);

    // Clamp distance to valid range
    distanceMeters = std::clamp( distanceMeters, 7.0f, 3250.0f );

    // Check if distance matches an exact entry
    for ( size_t i = 0; i < tableSize; ++i )
    {
        if ( std::abs( distanceMeters - AttenuationLookup[i].distance ) < 0.001f )
            return { AttenuationLookup[i].linear, AttenuationLookup[i].quadratic };
    }

    // Find the two entries to interpolate between
    size_t lowerIndex = 0;
    for ( size_t i = 0; i < tableSize - 1; ++i )
    {
        if ( distanceMeters >= AttenuationLookup[i].distance &&
             distanceMeters < AttenuationLookup[i + 1].distance )
        {
            lowerIndex = i;
            break;
        }
    }

    // Linear interpolation between the two entries
    const auto& lower = AttenuationLookup[lowerIndex];
    const auto& upper = AttenuationLookup[lowerIndex + 1];

    f32 distanceRange = upper.distance - lower.distance;
    f32 t = ( distanceMeters - lower.distance ) / distanceRange;

    f32 linear = lower.linear + t * ( upper.linear - lower.linear );
    f32 quadratic = lower.quadratic + t * ( upper.quadratic - lower.quadratic );

    return { linear, quadratic };
}

void Light::Update()
{
    auto [linear, quadratic] = GetAttenuationConstans( mDistance );
    mLinear = linear;
    mQuadratic = quadratic;
}

Light Light::CreateDirLight()
{
    Light light;
    light.mType = LightType::Directional;
    return light;
}

Light Light::CreatePointLight()
{
    Light light;
    light.mType = LightType::Point;
    return light;
}

Light Light::CreateSpotLight()
{
    Light light;
    light.mType = LightType::Spot;
    return light;
}

}