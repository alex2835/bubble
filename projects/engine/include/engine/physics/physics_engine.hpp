#pragma once 
#include "engine/physics/physics_object.hpp"
#include "engine/utils/timer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/pointer.hpp"
#include "btBulletDynamicsCommon.h"

namespace bubble
{

class PhysicsEngine
{
    Scope<btDefaultCollisionConfiguration> collisionConfiguration;
    Scope<btCollisionDispatcher> dispatcher;
    Scope<btBroadphaseInterface> overlappingPairCache;
    Scope<btSequentialImpulseConstraintSolver> solver;
    Scope<btDiscreteDynamicsWorld> dynamicsWorld;

public:
    PhysicsEngine();
    ~PhysicsEngine();

    void Update( DeltaTime dt );

    void Add( const PhysicsObject& obj );
    void Remove( const PhysicsObject& obj );
    void ClearWorld();

    void SetMass( PhysicsObject& obj, float mass );
};


}