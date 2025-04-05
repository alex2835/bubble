
#include "engine/physics/physics_object.hpp"

namespace bubble
{
PhysicsObject::PhysicsObject( const PhysicsObject& other )
{
    CopyFrom( other );
}

PhysicsObject& PhysicsObject::operator=( const PhysicsObject& other )
{
    if ( this != &other )
        CopyFrom( other );
    return *this;
}



void PhysicsObject::ApplyCentralImpulse( const vec3 impulse )
{
    body->activate( true );
    body->applyCentralImpulse( btVector3( impulse.x, impulse.y, impulse.z ) );
    //body->applyTorqueImpulse( btVector3( impulse.x, impulse.y, impulse.z ) );
}

vec3 PhysicsObject::GetPosition()
{
    btTransform trans;
    body->getMotionState()->getWorldTransform( trans );
    return vec3( trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ() );
}

vec3 PhysicsObject::GetRotation()
{
    btTransform trans;
    body->getMotionState()->getWorldTransform( trans );
    vec3 rads;
    trans.getRotation().getEulerZYX( rads.z, rads.y, rads.x );
    return vec3( glm::degrees( rads.x ), 
                 glm::degrees( rads.y ),
                 glm::degrees( rads.z ) );
}

void PhysicsObject::CopyFrom( const PhysicsObject& other )
{
    colShape = CreateScope<btSphereShape>( other.colShape->getRadius() );

    btScalar mass( other.body->getMass() );

    //rigidbody is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        colShape->calculateLocalInertia( mass, localInertia );

    btTransform transform;
    other.body->getMotionState()->getWorldTransform( transform );

    //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* myMotionState = new btDefaultMotionState( transform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, colShape.get(), localInertia );
    body = CreateScope<btRigidBody>( rbInfo );
}

}

