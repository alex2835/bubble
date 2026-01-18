#pragma once
#include <utility>
#include <algorithm>
#include "engine/renderer/shader.hpp"

namespace bubble
{
enum class LightType : i32
{
    Directional,
    Point,
    Spot
};


struct Light
{
    LightType mType = LightType::Directional;

    f32 mConstant = 1.0f;
    f32 mLinear = 0.0f;
    f32 mQuadratic = 0.0f;

    f32 mCutOff = 0.0f;
    f32 mOuterCutOff = 0.0f;
    f32 mDistance = 0.5f;
    f32 mBrightness = 1.0f;

    vec3 mColor = vec3( 1.0f );

    vec3 mPosition = vec3();
    vec3 mDirection = vec3();

    // Set light distance in meters (range: 7m to 3250m)
    // Automatically calculates linear and quadratic attenuation constants
    void SetDistance( f32 distanceMeters );

    static Light CreateDirLight( const vec3& direction = vec3( 0, -1.0f, 0 ),
                                 const vec3& color = vec3( 1.0f ) );

    // distance in meters (range: 7m to 3250m)
    static Light CreatePointLight( const vec3& position = vec3( 0.0f ),
                                   f32 distanceMeters = 50.0f,
                                   const vec3& color = vec3( 1.0f ) );

    // distance in meters (range: 7m to 3250m)
    static Light CreateSpotLight( const vec3& position = vec3(),
                                  const vec3& direction = vec3( 1.0f, 0.0f, 0.0f ),
                                  f32 distanceMeters = 50.0f,
                                  f32 cutoff = 12.5f, // cutoff and outer_cutoff in degrees
                                  f32 outer_cutoff = 17.5f,
                                  const vec3& color = vec3( 1.0f ) );
};
}