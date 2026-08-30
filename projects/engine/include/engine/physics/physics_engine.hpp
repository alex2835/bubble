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

// Bullet steps at a fixed rate and runs actions (the character controller) once
// per substep, so anything expressed per second converts through this.
constexpr f32 PHYSICS_FIXED_STEP = 1.0f / 60.0f;
// Allow the simulation to catch up over several substeps in one frame, otherwise
// physics silently runs in slow motion whenever the frame rate drops below 60.
constexpr i32 PHYSICS_MAX_SUBSTEPS = 10;

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
    PhysicsEngine( PhysicsEngine&& ) = default;
    PhysicsEngine& operator= ( PhysicsEngine&& ) = default;
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