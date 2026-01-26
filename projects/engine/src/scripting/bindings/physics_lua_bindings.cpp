#include "engine/loader/loader.hpp"
#include "engine/scripting/bindings/physics_lua_bindings.hpp"
#include "engine/physics/character_controller.hpp"
#include <sol/sol.hpp>

namespace bubble
{
void CreatePhysicsBindings( PhysicsEngine& physicsEngine, sol::state& lua )
{
    // Set mass done by PhysicsEngine remove and create new object
    sol::usertype<RigidBody> rigidBodyType = lua["RigidBody"];
    rigidBodyType["SetMass"] = [&]( RigidBody& obj, const float mass ) {
        physicsEngine.SetObjectMass( obj, mass );
    };


    // Ray casting
    lua.new_usertype<RayHitResult>(
        "RayHitResult",
        "hitPoint", &RayHitResult::hitPoint,
        "hitNormal", &RayHitResult::hitNormal,
        "hitFraction", &RayHitResult::hitFraction,
        "hitBody", &RayHitResult::hitBody,
        "entity", &RayHitResult::entity
    );

    lua["RaycastClosest"] = [&]( const vec3& from, const vec3& to ) {
        return physicsEngine.RaycastClosest( from, to );
    };

    lua["RaycastAll"] = [&]( const vec3& from, const vec3& to ) {
        return physicsEngine.RaycastAll( from, to );
    };
}
}