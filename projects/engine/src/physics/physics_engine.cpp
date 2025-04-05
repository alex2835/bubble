
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
    dynamicsWorld->addRigidBody( obj->body.get() );
}

Ref<PhysicsObject> PhysicsEngine::CreateSphere( vec3 pos, f32 mass, f32 radius )
{
    auto object = CreateRef<PhysicsObject>();

    object->colShape = CreateScope<btSphereShape>( radius );

    /// Create Dynamic Objects
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin( btVector3( pos.x, pos.y, pos.z ) );

    //rigidbody is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = ( mass != 0.f );
    btVector3 localInertia( 0, 0, 0 );
    if ( isDynamic )
        object->colShape->calculateLocalInertia( mass, localInertia );

    //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* myMotionState = new btDefaultMotionState( startTransform );
    btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, object->colShape.get(), localInertia );
    object->body = CreateScope<btRigidBody>( rbInfo );

    AddPhysicsObject( object );
    return object;
}

void PhysicsEngine::ClearWorld()
{
    for ( int i = 0; i < dynamicsWorld->getNumCollisionObjects(); i++ )
    {
        btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        btRigidBody* body = btRigidBody::upcast( obj );
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