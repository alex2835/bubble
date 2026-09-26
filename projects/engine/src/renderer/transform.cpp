#include "engine/renderer/transform.hpp"
#include <glm/gtx/euler_angles.hpp>

namespace bubble
{
quat Transform::FromEuler( const vec3& radians )
{
    return glm::angleAxis( radians.z, vec3( 0, 0, 1 ) ) *
           glm::angleAxis( radians.y, vec3( 0, 1, 0 ) ) *
           glm::angleAxis( radians.x, vec3( 1, 0, 0 ) );
}

vec3 Transform::ToEuler( const quat& rotation )
{
    f32 z, y, x;
    glm::extractEulerAngleZYX( glm::mat4_cast( rotation ), z, y, x );
    return vec3( x, y, z );
}

vec2 Transform::LookAngles() const
{
    // yaw * pitch takes +Z to (cos p sin y, -sin p, cos p cos y).
    const vec3 d = mRotation * vec3( 0, 0, 1 );
    return vec2( -std::asin( glm::clamp( d.y, -1.0f, 1.0f ) ), std::atan2( d.x, d.z ) );
}

void Transform::SetLookAngles( f32 pitch, f32 yaw )
{
    mRotation = glm::angleAxis( yaw, vec3( 0, 1, 0 ) ) * glm::angleAxis( pitch, vec3( 1, 0, 0 ) );
}

Transform Transform::FromMatrix( const mat4& matrix )
{
    Transform t;
    t.mPosition = vec3( matrix[3] );
    vec3 x = vec3( matrix[0] ), y = vec3( matrix[1] ), z = vec3( matrix[2] );
    t.mScale = vec3( glm::length( x ), glm::length( y ), glm::length( z ) );
    // A mirror is a negative scale; put it on X, as any axis would do.
    if ( glm::dot( glm::cross( x, y ), z ) < 0.0f )
        t.mScale.x = -t.mScale.x;
    for ( int i = 0; i < 3; i++ )
        if ( t.mScale[i] == 0.0f )
            return t;
    const mat3 rotation( x / t.mScale.x, y / t.mScale.y, z / t.mScale.z );
    t.mRotation = glm::normalize( glm::quat_cast( rotation ) );
    return t;
}

mat4 Transform::TransformMat() const
{
    return glm::translate( mat4( 1.0f ), mPosition ) * glm::mat4_cast( mRotation ) * glm::scale( mat4( 1.0f ), mScale );
}

mat4 Transform::ScaleMat() const
{
    return glm::scale( mat4( 1.0f ), mScale );
}

mat4 Transform::TranslationMat() const
{
    return glm::translate( mat4( 1.0f ), mPosition );
}

mat4 Transform::RotationMat() const
{
    return glm::mat4_cast( mRotation );
}

mat4 Transform::TranslationRotationMat() const
{
    return glm::translate( mat4( 1.0f ), mPosition ) * glm::mat4_cast( mRotation );
}

} // namespace bubble
