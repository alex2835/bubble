#include "engine/pch/pch.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
constexpr std::pair<f32, f32>
AttenuationLookup[] = {		   // Distance in Meters
        { 0.7f    ,1.8f      }, // 7	
        { 0.35f   ,0.44f     }, // 13	
        { 0.22f   ,0.20f     }, // 20	
        { 0.14f   ,0.07f     }, // 32	
        { 0.09f   ,0.032f    }, // 50	
        { 0.07f   ,0.017f    }, // 65	
        { 0.045f  ,0.0075f   }, // 100	
        { 0.027f  ,0.0028f   }, // 160	
        { 0.022f  ,0.0019f   }, // 200	
        { 0.014f  ,0.0007f   }, // 325	
        { 0.007f  ,0.0002f   }, // 600	
        { 0.0014f ,0.000007f }, // 3250
};


// Take distance between 0 and 1.0f (where 0 = 7m and 1.0f = 3250m)
std::pair<f32, f32> GetAttenuationConstans( f32 distance )
{
    f32 index = distance * 11.0f; // 11 is an array size
    f32 hight_coef = index - floor( index );
    f32 lower_coef = 1.0f - ( index - floor( index ) );

    // linear interpolation
    i32 nIndex = static_cast<i32>( index );
    auto first = AttenuationLookup[std::min( nIndex, 11 )];
    auto second = AttenuationLookup[std::min( ( nIndex + 1 ), 11 )];

    return { first.first * lower_coef + second.first * hight_coef,
            first.second * lower_coef + second.second * hight_coef };
}

void Light::SetDistance( f32 distance )
{
    auto [linear, quadratic] = GetAttenuationConstans( distance );
    mDistance = distance;
    mLinear = linear;
    mQuadratic = quadratic;
}

//void Light::Update()
//{
//    auto [linear, quadratic] = GetAttenuationConstans( Distance );
//    Linear = linear;
//    Quadratic = quadratic;
//    //__CutOff = cosf( radians( CutOff ) );
//    //__OuterCutOff = cosf( radians( OuterCutOff ) );
//
//    // Brightens compensation
//    if ( Type == LightType::PointLight || Type == LightType::SpotLight )
//    {
//        //__Brightness = Brightness * ( 7.0f - Distance * 5.0f );
//    }
//    else
//    {
//        //__Brightness = Brightness;
//    }
//}

Light Light::CreateDirLight( const vec3& direction, const vec3& color )
{
    Light light;
    light.mType = LightType::DirLight;
    light.mDirection = normalize( direction );
    light.mColor = color;
    return light;
}

Light Light::CreatePointLight( const vec3& position, f32 distance, const vec3& color )
{
    Light light;
    light.mPosition = position;
    light.mType = LightType::PointLight;
    light.mDistance = distance;
    light.mColor = color;
    return light;
}

Light Light::CreateSpotLight( const vec3& position,
                              const vec3& direction,
                              f32 distance,
                              f32 cutoff,
                              f32 outer_cutoff,
                              const vec3& color )
{
    Light light;
    light.mType = LightType::SpotLight;
    light.mPosition = position;
    light.mDirection = normalize( direction );
    light.mDistance = distance;
    light.mCutOff = cutoff;
    light.mOuterCutOff = outer_cutoff;
    light.mColor = color;
    return light;
}

}