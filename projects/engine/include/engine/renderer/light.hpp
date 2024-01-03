#pragma once
#include <utility>
#include <algorithm>
#include "glm/glm.hpp"
#include "engine/utils/imexp.hpp"
#include "engine/renderer/shader.hpp"

namespace bubble
{
enum class LightType
{
    DirLight,
    PointLight,
    SpotLight
};

// std_140 alignment
//struct GLSL_Light
//{
//    LightType Type = LightType::DirLight;
//    float __Brightness = 1.0f;
//
//    // Point
//    float Constant = 1.0f;
//    float Linear = 0.0f;
//    float Quadratic = 0.0f;
//
//    // Spot
//    float __CutOff = 0.0f;
//    float __OuterCutOff = 0.0f;
//    float __pad0 = 0;
//
//    glm::vec3 Color = glm::vec3( 1.0f );
//    float __pad1 = 0;
//
//    // Directional
//    glm::vec3 Direction = glm::vec3();
//    float __pad2 = 0;
//
//    glm::vec3 Position = glm::vec3();
//    float __pad3 = 0;
//};


struct BUBBLE_ENGINE_EXPORT Light
{
    LightType Type = LightType::DirLight;

    float Constant = 1.0f;
    float Linear = 0.0f;
    float Quadratic = 0.0f;

    float CutOff = 0.0f;
    float OuterCutOff = 0.0f;
    float Distance = 0.0f;
    float Brightness = 1.0f;

    glm::vec3 Color = glm::vec3( 1.0f );

    glm::vec3 Position = glm::vec3();
    glm::vec3 Direction = glm::vec3();

    void SetDistance( float distance );
    void Update();

    static Light CreateDirLight( const glm::vec3& direction = glm::vec3( -1.0f ), const glm::vec3& color = glm::vec3( 1.0f ) );

    // distance between 0 and 1.0f (where 1.0f is 3250m)
    static Light CreatePointLight( const glm::vec3& position = glm::vec3(), float distance = 0.5f, const glm::vec3& color = glm::vec3( 1.0f ) );

    // distance between 0 and 1.0f (where 1.0f is 3250m)
    static Light CreateSpotLight( const glm::vec3& position = glm::vec3(),
                                  const glm::vec3& direction = glm::vec3( 1.0f, 0.0f, 0.0f ),
                                  float distance = 0.5f,
                                  float cutoff = 12.5f,		// cutoff and outer_cutoff in degrees
                                  float outer_cutoff = 17.5f,
                                  const glm::vec3& color = glm::vec3( 1.0f ) );
};
}