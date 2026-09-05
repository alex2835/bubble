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


// The light uniform blocks, laid out to match the WGSL structs exactly.
//
// Light is the scene-facing type - degrees for the cone angles, a distance in
// meters. These are what the shader reads: cosines, and the attenuation
// constants Update() derived. The renderer converts on the way in.
//
// The explicit pads are load bearing. WGSL puts vec3 on a 16 byte boundary,
// but glm::vec3 is only 4 byte aligned, so the compiler will not insert that
// padding for us. The offset asserts below are what keep the two in step - a
// size-only check would pass on a field reorder that moved every vec3.
struct LightUniforms
{
    i32 mType = 0;
    f32 mBrightness = 1.0f;
    f32 mConstant = 1.0f;
    f32 mLinear = 1.0f;

    f32 mQuadratic = 1.0f;
    // Cosines, not degrees - the shader compares them against a dot product.
    f32 mCutOff = 0.0f;
    f32 mOuterCutOff = 0.0f;
    f32 mPad0 = 0.0f;

    vec3 mColor = vec3( 1.0f );
    f32 mPad1 = 0.0f;

    vec3 mDirection = vec3( 0, -1.0f, 0 );
    f32 mPad2 = 0.0f;

    vec3 mPosition = vec3( 0.0f );
    f32 mPad3 = 0.0f;
};
static_assert( sizeof( LightUniforms ) == 80, "LightUniforms must match the WGSL Light struct" );
static_assert( offsetof( LightUniforms, mColor ) == 32, "LightUniforms::mColor offset must match the WGSL Light struct" );
static_assert( offsetof( LightUniforms, mDirection ) == 48, "LightUniforms::mDirection offset must match the WGSL Light struct" );
static_assert( offsetof( LightUniforms, mPosition ) == 64, "LightUniforms::mPosition offset must match the WGSL Light struct" );


// How many lights the array holds, and where the camera is - the shader needs
// the view position for every specular term.
struct LightsInfoUniforms
{
    i32 mNumLights = 0;
    f32 mPad0 = 0.0f;
    f32 mPad1 = 0.0f;
    f32 mPad2 = 0.0f;

    vec3 mViewPos = vec3( 0.0f );
    f32 mPad3 = 0.0f;
};
static_assert( sizeof( LightsInfoUniforms ) == 32, "LightsInfoUniforms must match the WGSL struct" );
static_assert( offsetof( LightsInfoUniforms, mViewPos ) == 16, "LightsInfoUniforms::mViewPos offset must match the WGSL struct" );

}