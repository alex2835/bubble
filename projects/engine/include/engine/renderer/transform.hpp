#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"

namespace bubble
{
struct Transform
{
    vec3 mPosition = vec3( 0 );
    vec3 mRotation = vec3( 0 );
    vec3 mScale = vec3( 1 );

    mat4 TransformMat() const;
    mat4 ScaleMat() const;
    mat4 TranslationMat() const;
    mat4 RotationMat() const;
    mat4 TranslationRotationMat() const;
    vec3 Forward() const;
};

} // namespace bubble

