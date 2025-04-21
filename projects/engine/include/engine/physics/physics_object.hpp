#pragma once
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "btBulletDynamicsCommon.h"

namespace bubble
{
class PhysicsObject
{
public:
    PhysicsObject();
    PhysicsObject( const PhysicsObject& other );
    PhysicsObject& operator=( const PhysicsObject& other );
    PhysicsObject( PhysicsObject&& ) = default;
    PhysicsObject& operator=( PhysicsObject&& ) = default;

    void ApplyCentralImpulse( const vec3 impulse );

    void SetTransform( const vec3& pos, const vec3& rot );
    void GetTransform( vec3& pos, vec3& rot ) const;

    void ClearForces();

    btRigidBody* getBody();
    btCollisionShape* getShape();

    static Ref<PhysicsObject> CreateSphere( vec3 pos, f32 mass, f32 radius );
    static Ref<PhysicsObject> CreateBox( vec3 pos, f32 mass, vec3 halfExtends );
public:
    Scope<btCollisionShape> mColisionShape;
    Scope<btRigidBody> mBody;
private:
    void CopyFrom( const PhysicsObject& other );
};

}