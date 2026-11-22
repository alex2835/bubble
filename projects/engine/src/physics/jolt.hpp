#pragma once
// Jolt as the physics module's files see it, and the way between its maths
// and glm's. Included by src/physics alone.
#include "bubble/scene/entity.hpp"
#include "bubble/types/matrix.hpp"
// clang-format off
// Jolt.h comes first: it sets up what the rest of Jolt is built on.
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace bubble::physics
{
inline JPH::Vec3 ToJolt( vec3 v )
{
    return JPH::Vec3( v.x, v.y, v.z );
}

inline JPH::Quat ToJolt( const quat& q )
{
    return JPH::Quat( q.x, q.y, q.z, q.w );
}

inline vec3 FromJolt( JPH::Vec3Arg v )
{
    return vec3( v.GetX(), v.GetY(), v.GetZ() );
}

inline quat FromJolt( JPH::QuatArg q )
{
    return quat( q.GetW(), q.GetX(), q.GetY(), q.GetZ() );
}

inline Entity EntityOf( JPH::uint64 userData )
{
    return Entity( static_cast<Entity::entity_type>( userData ) );
}

inline JPH::uint64 UserDataOf( Entity entity )
{
    return static_cast<JPH::uint64>( static_cast<Entity::entity_type>( entity ) );
}

// A world matrix as a body takes it: where, which way, how large.
struct Pose
{
    vec3 mPosition{ 0.0f };
    quat mRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    vec3 mScale{ 1.0f };
};

Pose PoseOf( const mat4& matrix );
}
