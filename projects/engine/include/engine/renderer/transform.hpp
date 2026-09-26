#pragma once
#include "engine/types/number.hpp"
#include "engine/types/glm.hpp"

namespace bubble
{
// Where something is, relative to its parent: position, rotation, scale.
//
// The rotation is a quaternion. Euler angles are only how people and scripts
// write it - the inspector, Lua, old level files - and are converted at that
// edge, in the order the engine has always composed them: X first, then Y,
// then Z (the matrix is Rz * Ry * Rx). Stored as angles, a rotation locks up
// at 90 degrees of pitch, cannot be interpolated and has many spellings; as
// a quaternion it has none of that, and it is what Bullet and ozz speak.
//
// The matrix is built from these on demand and never stored as the truth:
// three separate parts stay exact under repeated edits, where a matrix
// multiplied in place drifts out of orthogonality.
struct Transform
{
    Transform() = default;
    // `rotation` in Euler radians, as above.
    Transform( vec3 position, vec3 rotation = vec3( 0 ), vec3 scale = vec3( 1 ) )
        : mPosition( position ), mRotation( FromEuler( rotation ) ), mScale( scale )
    {
    }
    Transform( vec3 position, quat rotation, vec3 scale = vec3( 1 ) )
        : mPosition( position ), mRotation( rotation ), mScale( scale )
    {
    }

    vec3 mPosition = vec3( 0 );
    quat mRotation = quat( 1.0f, 0.0f, 0.0f, 0.0f );
    vec3 mScale = vec3( 1 );

    // Radians, X then Y then Z. Reading angles back from a rotation gives
    // one of its spellings, not necessarily the one it was set with: Y
    // comes back within +-90 degrees.
    vec3 Euler() const { return ToEuler( mRotation ); }
    void SetEuler( const vec3& radians ) { mRotation = FromEuler( radians ); }
    static quat FromEuler( const vec3& radians );
    static vec3 ToEuler( const quat& rotation );

    // A camera's or a listener's reading of the rotation: pitch about X,
    // then yaw about Y, roll ignored - what a camera entity's rotation of
    // (pitch, yaw, 0) always meant. Read off where the rotation takes +Z,
    // so it is exact for any yaw, where Euler().y folds past 90 degrees.
    vec2 LookAngles() const; // (pitch, yaw)
    void SetLookAngles( f32 pitch, f32 yaw );

    // The same three parts out of a matrix built from them - and out of one
    // a gizmo has moved. Shear, which TRS cannot hold, is dropped.
    static Transform FromMatrix( const mat4& matrix );

    mat4 TransformMat() const;
    mat4 ScaleMat() const;
    mat4 TranslationMat() const;
    mat4 RotationMat() const;
    mat4 TranslationRotationMat() const;
};

} // namespace bubble
