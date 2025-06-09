#include "engine/pch/pch.hpp"
#include "engine/physics/physics_engine.hpp"

namespace bubble
{
PhysicsObject::PhysicsObject()
{

}

PhysicsEngine::PhysicsEngine()
{
    collisionConfiguration = CreateScope<btDefaultCollisionConfiguration>();
    dispatcher = CreateScope<btCollisionDispatcher>( collisionConfiguration.get() );
    overlappingPairCache = CreateScope<btDbvtBroadphase>();
    solver = CreateScope<btSequentialImpulseConstraintSolver>();
    dynamicsWorld = CreateScope<btDiscreteDynamicsWorld>( dispatcher.get(),
                                                          overlappingPairCache.get(),
                                                          solver.get(),
                                                          collisionConfiguration.get() );

    dynamicsWorld->setGravity( btVector3( 0, -10, 0 ) );
}

void PhysicsEngine::Add( const PhysicsObject& obj )
{
    dynamicsWorld->addRigidBody( obj.mBody.get() );
    obj.mBody->activate();
}

void PhysicsEngine::Remove( const PhysicsObject& obj )
{
    dynamicsWorld->removeRigidBody( obj.mBody.get() );
}

void PhysicsEngine::ClearWorld()
{
    for ( int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i-- )
    {
        btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        dynamicsWorld->removeCollisionObject( obj );
    }
}

void PhysicsEngine::Update( DeltaTime dt )
{
    dynamicsWorld->stepSimulation( dt.Seconds() );
}

PhysicsEngine::~PhysicsEngine()
{

}

}