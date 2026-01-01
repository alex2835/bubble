#pragma once
#include "engine/physics/physics_object.hpp"
#include "engine/utils/timer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/pointer.hpp"
#include "btBulletDynamicsCommon.h"
#include "recs/entity.hpp"

namespace bubble
{
using namespace recs;

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

    void Add( const PhysicsObject& obj, Entity entity );
    void Remove( const PhysicsObject& obj );
    void ClearWorld();

    void SetMass( PhysicsObject& obj, float mass );

    std::optional<RayHitResult> RaycastClosest( const vec3& from, const vec3& to ) const;
    std::vector<RayHitResult> RaycastAll( const vec3& from, const vec3& to ) const;
};


}