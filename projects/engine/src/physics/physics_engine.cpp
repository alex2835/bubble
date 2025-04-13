
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


    btCollisionShape* groundShape = new btBoxShape( btVector3( btScalar( 100. ), btScalar( 1. ), btScalar( 100. ) ) );
    btTransform groundTransform;
    groundTransform.setIdentity();
    groundTransform.setRotation( btQuaternion( btVector3( 0, 0, 1 ), SIMD_PI / 12 ) );
    groundTransform.setOrigin( btVector3( 0, 0, 0 ) );
    btScalar mass( 0. );
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        groundShape->calculateLocalInertia( mass, localInertia );
    btDefaultMotionState* myMotionState = new btDefaultMotionState( groundTransform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, groundShape, localInertia );
    btRigidBody* body = new btRigidBody( rbInfo );
    dynamicsWorld->addRigidBody( body );

    dynamicsWorld->setGravity( btVector3( 0, -10, 0 ) );
}

void PhysicsEngine::AddPhysicsObject( const Ref<PhysicsObject>& obj )
{
    dynamicsWorld->addRigidBody( obj->mBody.get() );
    obj->mBody->activate();
}

void PhysicsEngine::ClearWorld()
{
    for ( int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i-- )
    {
        btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        //btRigidBody* body = btRigidBody::upcast( obj );
        //if ( body && body->getMotionState() )
        //{
        //    delete body->getMotionState();
        //}
        dynamicsWorld->removeCollisionObject( obj );
        //delete obj;
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