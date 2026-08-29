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
    rigidBodyType["set_mass"] = [&]( RigidBody& obj, const float mass ) {
        physicsEngine.SetObjectMass( obj, mass );
    };


    // Ray casting
    lua.new_usertype<RayHitResult>(
        "RayHitResult",
        "hit_point", &RayHitResult::hitPoint,
        "hit_normal", &RayHitResult::hitNormal,
        "hit_fraction", &RayHitResult::hitFraction,
        "hit_body", &RayHitResult::hitBody,
        "entity", &RayHitResult::entity
    );

    lua["raycast_closest"] = [&]( const vec3& from, const vec3& to ) {
        return physicsEngine.RaycastClosest( from, to );
    };

    lua["raycast_all"] = [&]( const vec3& from, const vec3& to ) {
        return physicsEngine.RaycastAll( from, to );
    };
}
}