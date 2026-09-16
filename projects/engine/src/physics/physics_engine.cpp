#include "engine/pch/pch.hpp"
#include "engine/physics/physics_engine.hpp"
#include "engine/physics/character_controller.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h"
#include "LinearMath/btThreads.h"
#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#endif

namespace bubble
{

#if BT_THREADSAFE
// Bullet's task scheduler is process-global and owns its worker threads, so it
// is created once and shared by every PhysicsEngine (the engine rebuilds one
// on each OnEnd). Bullet's own Win32/POSIX pool; no OpenMP/TBB dependency.
static int InitTaskScheduler()
{
    static btITaskScheduler* scheduler = []
    {
#if defined(_WIN32)
        // btThreadSupportWin32 pins the *calling* thread to physical core 0 as a
        // side effect of spawning the workers and never undoes it - which would
        // leave the whole editor / render loop confined to one core. Remember
        // the process mask and put it back once the workers exist.
        DWORD_PTR processMask = 0, systemMask = 0;
        const bool haveMask = GetProcessAffinityMask( GetCurrentProcess(), &processMask, &systemMask ) and processMask != 0;
#endif
        // Thread count is Bullet's own default: one per physical core, the
        // calling thread counted as worker 0.
        btITaskScheduler* s = btCreateDefaultTaskScheduler();
#if defined(_WIN32)
        if ( haveMask )
            SetThreadAffinityMask( GetCurrentThread(), processMask );
#endif
        btSetTaskScheduler( s );
        return s;
    }();
    return scheduler->getNumThreads();
}
#endif

PhysicsEngine::PhysicsEngine()
{
    collisionConfiguration = CreateScope<btDefaultCollisionConfiguration>();
    overlappingPairCache = CreateScope<btDbvtBroadphase>();

#if BT_THREADSAFE
    const int numThreads = InitTaskScheduler();
    dispatcher = CreateScope<btCollisionDispatcherMt>( collisionConfiguration.get() );
    // One solver per thread means island solving never spin-waits for a free one.
    solverPool = CreateScope<btConstraintSolverPoolMt>( numThreads );
    solver = CreateScope<btSequentialImpulseConstraintSolverMt>();
    dynamicsWorld = CreateScope<btDiscreteDynamicsWorldMt>(
        dispatcher.get(),
        overlappingPairCache.get(),
        solverPool.get(),
        solver.get(),
        collisionConfiguration.get()
    );
#else
    dispatcher = CreateScope<btCollisionDispatcher>( collisionConfiguration.get() );
    solver = CreateScope<btSequentialImpulseConstraintSolver>();
    dynamicsWorld = CreateScope<btDiscreteDynamicsWorld>(
        dispatcher.get(),
        overlappingPairCache.get(),
        solver.get(),
        collisionConfiguration.get()
    );
#endif

    dynamicsWorld->setGravity( btVector3( 0, -10, 0 ) );

    btOverlappingPairCache* pairCache = overlappingPairCache->getOverlappingPairCache();
    pairCache->setInternalGhostPairCallback( new btGhostPairCallback() );
}

void PhysicsEngine::Add( const RigidBody& obj, Entity entity )
{
    obj.mBody->setUserPointer( new Entity( entity ) );
    dynamicsWorld->addRigidBody( obj.mBody.get(),
                                 btBroadphaseProxy::DefaultFilter,
                                 btBroadphaseProxy::AllFilter );
    obj.mBody->activate();
}

void PhysicsEngine::Remove( const RigidBody& obj )
{
    if ( obj.mBody->getUserPointer() )
    {
        delete static_cast<Entity*>( obj.mBody->getUserPointer() );
        obj.mBody->setUserPointer( nullptr );
    }
    dynamicsWorld->removeRigidBody( obj.mBody.get() );
}

void PhysicsEngine::Add( CharacterController& controller, Entity entity )
{
    controller.mGhostObject->setUserPointer( new Entity( entity ) );
    dynamicsWorld->addCollisionObject( controller.mGhostObject.get(),
                                       btBroadphaseProxy::CharacterFilter,
                                       btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter );
    dynamicsWorld->addAction( controller.mController.get() );
}

void PhysicsEngine::Remove( CharacterController& controller )
{
    if ( controller.mGhostObject->getUserPointer() )
    {
        delete static_cast<Entity*>( controller.mGhostObject->getUserPointer() );
        controller.mGhostObject->setUserPointer( nullptr );
    }
    dynamicsWorld->removeAction( controller.mController.get() );
    dynamicsWorld->removeCollisionObject( controller.mGhostObject.get() );
}

void PhysicsEngine::ClearWorld()
{
    for ( int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i-- )
    {
        btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
        if ( obj->getUserPointer() )
        {
            delete static_cast<Entity*>( obj->getUserPointer() );
            obj->setUserPointer( nullptr );
        }
        dynamicsWorld->removeCollisionObject( obj );
    }
}

void PhysicsEngine::Update( DeltaTime dt )
{
    dynamicsWorld->stepSimulation( dt.Seconds(), PHYSICS_MAX_SUBSTEPS, PHYSICS_FIXED_STEP );
}

void PhysicsEngine::SetObjectMass( RigidBody& obj, float mass )
{
    // Remove from world
    dynamicsWorld->removeRigidBody( obj.getBody() );

    // Update mass properties
    btVector3 newInertia( 0, 0, 0 );
    if ( mass != 0.0f )  // Only dynamic bodies have inertia
        obj.getShape()->calculateLocalInertia( mass, newInertia );

    obj.getBody()->setMassProps( mass, newInertia );
    obj.getBody()->updateInertiaTensor();

    // Re-add to world
    dynamicsWorld->addRigidBody( obj.getBody() );

    // Activate to ensure changes take effect
    obj.getBody()->activate( true );
}

std::optional<RayHitResult> PhysicsEngine::RaycastClosest( const vec3& from, const vec3& to ) const
{
    std::vector<RayHitResult> results;

    btVector3 rayFrom( from.x, from.y, from.z );
    btVector3 rayTo( to.x, to.y, to.z );

    btCollisionWorld::ClosestRayResultCallback rayCallback( rayFrom, rayTo );
    dynamicsWorld->rayTest( rayFrom, rayTo, rayCallback );

    if ( rayCallback.hasHit() )
    {
        RayHitResult result;
        result.hitPoint = vec3( rayCallback.m_hitPointWorld.x(),
                                rayCallback.m_hitPointWorld.y(),
                                rayCallback.m_hitPointWorld.z() );
        result.hitNormal = vec3( rayCallback.m_hitNormalWorld.x(),
                                 rayCallback.m_hitNormalWorld.y(),
                                 rayCallback.m_hitNormalWorld.z() );
        result.hitFraction = rayCallback.m_closestHitFraction;
        result.hitBody = const_cast<btRigidBody*>( btRigidBody::upcast( rayCallback.m_collisionObject ) );

        if ( rayCallback.m_collisionObject->getUserPointer() )
            result.entity = *static_cast<Entity*>( rayCallback.m_collisionObject->getUserPointer() );

        return result;
    }
    return std::nullopt;
}

std::vector<RayHitResult> PhysicsEngine::RaycastAll( const vec3& from, const vec3& to ) const
{
    std::vector<RayHitResult> results;

    btVector3 rayFrom( from.x, from.y, from.z );
    btVector3 rayTo( to.x, to.y, to.z );

    btCollisionWorld::AllHitsRayResultCallback rayCallback( rayFrom, rayTo );
    dynamicsWorld->rayTest( rayFrom, rayTo, rayCallback );

    if ( rayCallback.hasHit() )
    {
        for ( int i = 0; i < rayCallback.m_hitPointWorld.size(); i++ )
        {
            RayHitResult result;
            result.hitPoint = vec3( rayCallback.m_hitPointWorld[i].x(),
                                    rayCallback.m_hitPointWorld[i].y(),
                                    rayCallback.m_hitPointWorld[i].z() );
            result.hitNormal = vec3( rayCallback.m_hitNormalWorld[i].x(),
                                     rayCallback.m_hitNormalWorld[i].y(),
                                     rayCallback.m_hitNormalWorld[i].z() );
            result.hitFraction = rayCallback.m_hitFractions[i];
            result.hitBody = const_cast<btRigidBody*>( btRigidBody::upcast( rayCallback.m_collisionObjects[i] ) );

            if ( rayCallback.m_collisionObjects[i]->getUserPointer() )
                result.entity = *static_cast<Entity*>( rayCallback.m_collisionObjects[i]->getUserPointer() );

            results.push_back( result );
        }
    }

    return results;
}

PhysicsEngine::~PhysicsEngine()
{
}

}