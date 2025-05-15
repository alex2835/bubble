#pragma once
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/number.hpp"
#include "engine/utils/geometry.hpp"
#include "btBulletDynamicsCommon.h"

namespace bubble
{
class PhysicsObject
{
public:
    PhysicsObject( const PhysicsObject& other );
    PhysicsObject& operator=( const PhysicsObject& other );
    PhysicsObject( PhysicsObject&& ) = default;
    PhysicsObject& operator=( PhysicsObject&& ) = default;

    void ApplyCentralImpulse( const vec3 impulse );

    void SetTransform( const vec3& pos, const vec3& rot );
    void GetTransform( vec3& pos, vec3& rot ) const;

    void ClearForces();

    btRigidBody* getBody();
    const btRigidBody* getBody() const;
    btCollisionShape* getShape();
    const btCollisionShape* getShape() const;

    static PhysicsObject CreateSphere( f32 mass, f32 radius );
    static PhysicsObject CreateBox( f32 mass, vec3 halfExtends );


private:
    PhysicsObject();

public:
    Scope<btCollisionShape> mColisionShape;
    Scope<btRigidBody> mBody;
    ShapeData mShapeData;
private:
    void CopyFrom( const PhysicsObject& other );
};

}