#pragma once
#include <utility>
#include <algorithm>
#include "engine/renderer/shader.hpp"

namespace bubble
{
enum class LightType : i32
{
    DirLight,
    PointLight,
    SpotLight
};

// std_140 alignment
//struct GLSL_Light
//{
//    LightType Type = LightType::DirLight;
//    f32 __Brightness = 1.0f;
//
//    // Point
//    f32 Constant = 1.0f;
//    f32 Linear = 0.0f;
//    f32 Quadratic = 0.0f;
//
//    // Spot
//    f32 __CutOff = 0.0f;
//    f32 __OuterCutOff = 0.0f;
//    f32 __pad0 = 0;
//
//    vec3 Color = vec3( 1.0f );
//    f32 __pad1 = 0;
//
//    // Directional
//    vec3 Direction = vec3();
//    f32 __pad2 = 0;
//
//    vec3 Position = vec3();
//    f32 __pad3 = 0;
//};


struct Light
{
    LightType mType = LightType::DirLight;

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

    void SetDistance( f32 distance );
    //void Update();

    static Light CreateDirLight( const vec3& direction = vec3( 0, -1.0f, 0 ),
                                 const vec3& color = vec3( 1.0f ) );

    // distance between 0 and 1.0f (where 1.0f is 3250m)
    static Light CreatePointLight( const vec3& position = vec3( 0.0f ),
                                   f32 distance = 0.5f,
                                   const vec3& color = vec3( 1.0f ) );

    // distance between 0 and 1.0f (where 1.0f is 3250m)
    static Light CreateSpotLight( const vec3& position = vec3(),
                                  const vec3& direction = vec3( 1.0f, 0.0f, 0.0f ),
                                  f32 distance = 0.5f,
                                  f32 cutoff = 12.5f, // cutoff and outer_cutoff in degrees
                                  f32 outer_cutoff = 17.5f,
                                  const vec3& color = vec3( 1.0f ) );
};
}