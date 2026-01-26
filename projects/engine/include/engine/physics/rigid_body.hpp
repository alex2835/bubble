#pragma once
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/number.hpp"
#include "engine/utils/geometry.hpp"
#include "btBulletDynamicsCommon.h"

namespace bubble
{
class RigidBody
{
public:
    RigidBody( const RigidBody& other );
    RigidBody& operator=( const RigidBody& other );
    RigidBody( RigidBody&& ) = default;
    RigidBody& operator=( RigidBody&& ) = default;

    void SetMass( float mass );
    float GetMass();

    void SetFriction( float friction );
    float GetFriction();

    void ApplyCentralImpulse( const vec3 impulse );
    void ApplyTorqueImpulse( const vec3 impulse );

    void SetTransform( const vec3& pos, const vec3& rot );
    void GetTransform( vec3& pos, vec3& rot ) const;

    void ClearForces();

    btRigidBody* getBody();
    const btRigidBody* getBody() const;
    btCollisionShape* getShape();
    const btCollisionShape* getShape() const;

    const ShapeData& GetShapeData() const { return mShapeData; }

    static RigidBody CreateSphere( f32 mass, f32 radius );
    static RigidBody CreateBox( f32 mass, vec3 halfExtends );
    static RigidBody CreateCapsule( f32 mass, f32 radius, f32 height );


private:
    RigidBody();
    friend class PhysicsEngine;

public:
    Scope<btCollisionShape> mColisionShape;
    Scope<btRigidBody> mBody;
    ShapeData mShapeData;
private:
    void CopyFrom( const RigidBody& other );
};

}
