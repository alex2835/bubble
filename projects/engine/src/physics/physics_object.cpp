
#include "engine/physics/physics_object.hpp"
#include <stdexcept>
#include <iostream>

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
    mBody->activate( true );
    mBody->applyCentralImpulse( btVector3( impulse.x, impulse.y, impulse.z ) );
    //body->applyTorqueImpulse( btVector3( impulse.x, impulse.y, impulse.z ) );
}

void PhysicsObject::SetTransform( const vec3& pos, const vec3& rot )
{
    btTransform transform;
    transform.setOrigin( btVector3( pos.x, pos.y, pos.z ) );

    btQuaternion q;
    q.setEulerZYX( rot.z, rot.y, rot.x );
    transform.setRotation( q );

    mBody->setWorldTransform( transform );
    mBody->getMotionState()->setWorldTransform( transform );
}

void PhysicsObject::GetTransform( vec3& pos, vec3& rot ) const
{
    btTransform transform;
    mBody->getMotionState()->getWorldTransform( transform );
    pos = vec3( transform.getOrigin().getX(), 
                transform.getOrigin().getY(),
                transform.getOrigin().getZ() );

    btVector3 rads;
    transform.getRotation().getEulerZYX( rads[0], rads[1], rads[2] );
    rot = vec3( rads.z(), rads.y(), rads.x() );
}

void PhysicsObject::ClearForces()
{
    mBody->setLinearVelocity( btVector3( 0, 0, 0 ) );
    mBody->setAngularVelocity( btVector3( 0, 0, 0 ) );
    mBody->clearForces();
}

btRigidBody* PhysicsObject::getBody()
{
    return mBody.get();
}

const btRigidBody* PhysicsObject::getBody() const
{
    return mBody.get();
}

btCollisionShape* PhysicsObject::getShape()
{
    return mColisionShape.get();
}

const btCollisionShape* PhysicsObject::getShape() const
{
    return mColisionShape.get();
}

void PhysicsObject::CopyFrom( const PhysicsObject& other )
{
    switch ( other.mColisionShape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphere = static_cast<btSphereShape*>( other.mColisionShape.get() );
            mColisionShape = CreateScope<btSphereShape>( sphere->getRadius() );
            break;
        }
        case BOX_SHAPE_PROXYTYPE:
        {
            auto box = static_cast<btBoxShape*>( other.mColisionShape.get() );
            mColisionShape = CreateScope<btBoxShape>( box->getHalfExtentsWithoutMargin() );
        	break;
        }
        default:
            throw std::runtime_error( "Not supported collision shape type");
    }


    btScalar mass( other.mBody->getMass() );

    // Rigid body is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        mColisionShape->calculateLocalInertia( mass, localInertia );

    btTransform transform;
    other.mBody->getMotionState()->getWorldTransform( transform );

    //using motion state is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* myMotionState = new btDefaultMotionState( transform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, mColisionShape.get(), localInertia );
    mBody = CreateScope<btRigidBody>( rbInfo );
}



PhysicsObject PhysicsObject::CreateSphere( vec3 pos, f32 mass, f32 radius )
{
    PhysicsObject object;

    object.mColisionShape = CreateScope<btSphereShape>( radius );

    /// Create Dynamic Objects
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin( btVector3( pos.x, pos.y, pos.z ) );

    // rigid body is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        object.mColisionShape->calculateLocalInertia( mass, localInertia );

    //using motion state is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* myMotionState = new btDefaultMotionState( startTransform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, object.mColisionShape.get(), localInertia );
    object.mBody = CreateScope<btRigidBody>( rbInfo );
    return object;
}

PhysicsObject PhysicsObject::CreateBox( vec3 pos, f32 mass, vec3 halfExtends )
{
    PhysicsObject object;
    object.mColisionShape = CreateScope<btBoxShape>( btVector3( halfExtends.x,
                                                                 halfExtends.y, 
                                                                 halfExtends.z ) );
    btTransform groundTransform;
    groundTransform.setIdentity();
    groundTransform.setOrigin( btVector3( pos.x, pos.y, pos.z ) );
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        object.mColisionShape->calculateLocalInertia( mass, localInertia );
    btDefaultMotionState* myMotionState = new btDefaultMotionState( groundTransform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, object.mColisionShape.get(), localInertia );
    object.mBody = CreateScope<btRigidBody>( rbInfo );
    return object;
}

}

