#include "engine/pch/pch.hpp"
#include "engine/renderer/light.hpp"

namespace bubble
{
// The attenuation constants, from the range the light should reach.
//
// This replaces the twelve row Ogre3D lookup table that used to live here. That
// table is a rounded sampling of exactly these two curves - it reproduces every
// one of its rows to within its own rounding - so the table, the exact match
// scan and the interpolation between rows were all reconstructing something
// with a closed form.
//
// Interpolating was also the worse of the two: 1/d^2 is convex, so a straight
// line between samples sits above the true curve and overestimates the
// quadratic term. In the sparse upper half of the table that was severe - at
// d = 2000 the interpolated quadratic came out 423% too high, leaving the light
// five times darker at its nominal range than the same range asked for lower
// down.
//
// Solving 1/( 1 + linear*d + quadratic*d^2 ) at d gives ~1.24% for any d, so
// mDistance means "the radius at which this light has faded to about 1% of full
// brightness". That is a falloff shape in world units, not a physical length -
// the constants carry no unit of their own.
void Light::Update()
{
    // The table started at 7, which is why anything smaller used to be clamped
    // up to it. Nothing in the maths needs that floor; only a division by zero
    // has to be kept out.
    const f32 distance = std::max( mDistance, 0.0001f );

    mConstant = 1.0f;
    mLinear = 4.5f / distance;
    mQuadratic = 75.0f / ( distance * distance );
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