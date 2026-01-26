#pragma once
#include "engine/types/pointer.hpp"
#include "engine/types/glm.hpp"
#include "engine/types/number.hpp"
#include "engine/utils/geometry.hpp"
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"

namespace bubble
{

class CharacterController
{
public:
    CharacterController( f32 radius, f32 height, f32 stepHeight = 0.35f );
    ~CharacterController() = default;

    CharacterController( const CharacterController& other );
    CharacterController& operator=( const CharacterController& other );
    CharacterController( CharacterController&& ) = default;
    CharacterController& operator=( CharacterController&& ) = default;

    // Movement
    void SetWalkDirection( const vec3& direction );
    void SetVelocityForTimeInterval( const vec3& velocity, f32 timeInterval );
    void Jump( const vec3& direction = vec3( 0, 1, 0 ) );
    void Warp( const vec3& position );

    // State queries
    bool IsOnGround() const;
    vec3 GetPosition() const;
    vec3 GetLinearVelocity() const;

    // Configuration
    void SetMaxJumpHeight( f32 height );
    void SetJumpSpeed( f32 speed );
    void SetFallSpeed( f32 speed );
    void SetGravity( const vec3& gravity );
    void SetMaxSlope( f32 slopeRadians );
    void SetStepHeight( f32 height );

    // Getters
    f32 GetRadius() const { return mRadius; }
    f32 GetHeight() const { return mHeight; }
    f32 GetStepHeight() const { return mStepHeight; }
    const ShapeData& GetShapeData() const { return mShapeData; }

    btPairCachingGhostObject* GetGhostObject() { return mGhostObject.get(); }
    const btPairCachingGhostObject* GetGhostObject() const { return mGhostObject.get(); }

private:
    friend class PhysicsEngine;

    f32 mRadius;
    f32 mHeight;
    f32 mStepHeight;
    ShapeData mShapeData;

    Scope<btCapsuleShape> mShape;
    Scope<btPairCachingGhostObject> mGhostObject;
    Scope<btKinematicCharacterController> mController;
};

}
