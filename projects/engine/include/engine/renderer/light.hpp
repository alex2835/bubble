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
    f32 mLinear = 1.0f;
    f32 mQuadratic = 1.0f;

    f32 mCutOff = 12.5f;
    f32 mOuterCutOff = 17.5f;
    f32 mDistance = 50.0f;
    f32 mBrightness = 1.0f;

    vec3 mColor = vec3( 1.0f );

    vec3 mPosition = vec3();
    vec3 mDirection = vec3( 0, -1.0f, 0 );

    // Set light distance in meters (range: 7m to 3250m)
    // Automatically calculates linear and quadratic attenuation constants
    void Update();

    static Light CreateDirLight();
    static Light CreatePointLight();
    static Light CreateSpotLight();
};
}