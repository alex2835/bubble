#pragma once
#include "engine/physics/rigid_body.hpp"
#include "engine/utils/timer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/pointer.hpp"
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"
#include "recs/entity.hpp"

namespace bubble
{
using namespace recs;

class CharacterController;

struct RayHitResult
{
    vec3 hitPoint;
    vec3 hitNormal;
    float hitFraction = 0;
    btRigidBody* hitBody = nullptr;
    Entity entity;
};

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

    void Add( const RigidBody& obj, Entity entity );
    void Remove( const RigidBody& obj );

    void Add( CharacterController& controller, Entity entity );
    void Remove( CharacterController& controller );

    void ClearWorld();

    void SetObjectMass( RigidBody& obj, float mass );

    std::optional<RayHitResult> RaycastClosest( const vec3& from, const vec3& to ) const;
    std::vector<RayHitResult> RaycastAll( const vec3& from, const vec3& to ) const;

    btDiscreteDynamicsWorld* GetWorld() { return dynamicsWorld.get(); }
};


}