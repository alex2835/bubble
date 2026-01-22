#include "engine/renderer/transform.hpp"

namespace bubble
{
// Transform
mat4 Transform::TransformMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    transform = glm::scale( transform, mScale );
    return transform;
}

mat4 Transform::ScaleMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::scale( transform, mScale );
    return transform;
}

mat4 Transform::TranslationMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    return transform;
}

mat4 Transform::RotationMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    return transform;
}

mat4 Transform::TranslationRotationMat() const
{
    auto transform = mat4( 1.0f );
    transform = glm::translate( transform, mPosition );
    transform = glm::rotate( transform, mRotation.z, vec3( 0, 0, 1 ) );
    transform = glm::rotate( transform, mRotation.y, vec3( 0, 1, 0 ) );
    transform = glm::rotate( transform, mRotation.x, vec3( 1, 0, 0 ) );
    return transform;
}

vec3 Transform::Forward() const
{
    return normalize( vec3(
        cos( mRotation.y ) * cos( mRotation.x ),
        sin( mRotation.x ),
        sin( mRotation.y ) * cos( mRotation.x )
    ) );
}

} // namespace bubble