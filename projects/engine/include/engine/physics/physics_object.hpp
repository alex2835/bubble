#pragma once
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "btBulletDynamicsCommon.h"

namespace bubble
{
class PhysicsObject
{
    //btCollisionShape* colShape = nullptr;
    Scope<btSphereShape> colShape;
    Scope<btRigidBody> body;
    friend class PhysicsEngine;
public:
    PhysicsObject();
    PhysicsObject( const PhysicsObject& other );
    PhysicsObject& operator=( const PhysicsObject& other );
    PhysicsObject( PhysicsObject&& ) = default;
    PhysicsObject& operator=( PhysicsObject&& ) = default;

    void ApplyCentralImpulse( const vec3 impulse );

    vec3 GetPosition();
    vec3 GetRotation();

private:
    void CopyFrom( const PhysicsObject& other );

};

}