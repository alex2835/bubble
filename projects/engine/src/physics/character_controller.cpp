#include "engine/pch/pch.hpp"
#include "engine/physics/character_controller.hpp"

namespace bubble
{

CharacterController::CharacterController( f32 radius, f32 height, f32 stepHeight /*= 0.35f*/ )
    : mRadius( radius )
    , mHeight( height )
    , mStepHeight( stepHeight )
{
    mShapeData = GenerateCapsuleLinesShape( radius, height );

    // Create capsule shape (Y-axis aligned)
    mShape = CreateScope<btCapsuleShape>( radius, height );

    // Create ghost object for collision detection
    mGhostObject = CreateScope<btPairCachingGhostObject>();
    mGhostObject->setCollisionShape( mShape.get() );
    mGhostObject->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT );
    mGhostObject->setActivationState( DISABLE_DEACTIVATION );

    // Set initial transform
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin( btVector3( 0, 0, 0 ) );
    mGhostObject->setWorldTransform( startTransform );

    // Create character controller
    // upAxis = 1 means Y-axis is up
    mController = CreateScope<btKinematicCharacterController>(
        mGhostObject.get(),
        mShape.get(),
        stepHeight,
        btVector3( 0, 1, 0 )  // up vector
    );

    // Default configuration (use stored member values)
    mController->setGravity( btVector3( mGravity.x, mGravity.y, mGravity.z ) );
    mController->setJumpSpeed( mJumpSpeed );
    mController->setFallSpeed( mFallSpeed );
    mController->setMaxSlope( mMaxSlopeRadians );
}

CharacterController::CharacterController( const CharacterController& other )
    : mRadius( other.mRadius )
    , mHeight( other.mHeight )
    , mStepHeight( other.mStepHeight )
    , mJumpSpeed( other.mJumpSpeed )
    , mFallSpeed( other.mFallSpeed )
    , mMaxSlopeRadians( other.mMaxSlopeRadians )
    , mGravity( other.mGravity )
    , mShapeData( other.mShapeData )
{
    // Create capsule shape (Y-axis aligned)
    mShape = CreateScope<btCapsuleShape>( mRadius, mHeight );

    // Create ghost object for collision detection
    mGhostObject = CreateScope<btPairCachingGhostObject>();
    mGhostObject->setCollisionShape( mShape.get() );
    mGhostObject->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT );

    // Copy transform from source
    mGhostObject->setWorldTransform( other.mGhostObject->getWorldTransform() );

    // Create character controller
    mController = CreateScope<btKinematicCharacterController>(
        mGhostObject.get(),
        mShape.get(),
        mStepHeight,
        btVector3( 0, 1, 0 )  // up vector
    );

    // Copy configuration from source
    mController->setGravity( btVector3( mGravity.x, mGravity.y, mGravity.z ) );
    mController->setJumpSpeed( mJumpSpeed );
    mController->setFallSpeed( mFallSpeed );
    mController->setMaxSlope( mMaxSlopeRadians );
}

CharacterController& CharacterController::operator=( const CharacterController& other )
{
    if ( this == &other )
        return *this;

    mRadius = other.mRadius;
    mHeight = other.mHeight;
    mStepHeight = other.mStepHeight;
    mJumpSpeed = other.mJumpSpeed;
    mFallSpeed = other.mFallSpeed;
    mMaxSlopeRadians = other.mMaxSlopeRadians;
    mGravity = other.mGravity;
    mShapeData = other.mShapeData;

    // Create capsule shape (Y-axis aligned)
    mShape = CreateScope<btCapsuleShape>( mRadius, mHeight );

    // Create ghost object for collision detection
    mGhostObject = CreateScope<btPairCachingGhostObject>();
    mGhostObject->setCollisionShape( mShape.get() );
    mGhostObject->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT );

    // Copy transform from source
    mGhostObject->setWorldTransform( other.mGhostObject->getWorldTransform() );

    // Create character controller
    mController = CreateScope<btKinematicCharacterController>(
        mGhostObject.get(),
        mShape.get(),
        mStepHeight,
        btVector3( 0, 1, 0 )  // up vector
    );

    // Copy configuration from source
    mController->setGravity( btVector3( mGravity.x, mGravity.y, mGravity.z ) );
    mController->setJumpSpeed( mJumpSpeed );
    mController->setFallSpeed( mFallSpeed );
    mController->setMaxSlope( mMaxSlopeRadians );

    return *this;
}

void CharacterController::SetWalkDirection( const vec3& direction )
{
    mController->setWalkDirection( btVector3( direction.x, direction.y, direction.z ) );
}

void CharacterController::SetVelocityForTimeInterval( const vec3& velocity, f32 timeInterval )
{
    mController->setVelocityForTimeInterval( btVector3( velocity.x, velocity.y, velocity.z ), timeInterval );
}

void CharacterController::Jump( const vec3& direction /*= vec3( 0, 1, 0 )*/ )
{
    if ( mController->onGround() )
    {
        mController->jump( btVector3( direction.x, direction.y, direction.z ) );
    }
}

void CharacterController::Warp( const vec3& position )
{
    mController->warp( btVector3( position.x, position.y, position.z ) );
}

bool CharacterController::IsOnGround() const
{
    return mController->onGround();
}

vec3 CharacterController::GetPosition() const
{
    btTransform transform = mGhostObject->getWorldTransform();
    btVector3 origin = transform.getOrigin();
    return vec3( origin.x(), origin.y(), origin.z() );
}

vec3 CharacterController::GetLinearVelocity() const
{
    btVector3 vel = mController->getLinearVelocity();
    return vec3( vel.x(), vel.y(), vel.z() );
}

void CharacterController::SetMaxJumpHeight( f32 height )
{
    mController->setMaxJumpHeight( height );
}

void CharacterController::SetJumpSpeed( f32 speed )
{
    mJumpSpeed = speed;
    mController->setJumpSpeed( speed );
}

void CharacterController::SetFallSpeed( f32 speed )
{
    mFallSpeed = speed;
    mController->setFallSpeed( speed );
}

void CharacterController::SetGravity( const vec3& gravity )
{
    mGravity = gravity;
    mController->setGravity( btVector3( gravity.x, gravity.y, gravity.z ) );
}

void CharacterController::SetMaxSlope( f32 slopeRadians )
{
    mMaxSlopeRadians = slopeRadians;
    mController->setMaxSlope( slopeRadians );
}

void CharacterController::SetStepHeight( f32 height )
{
    mStepHeight = height;
    mController->setStepHeight( height );
}

}
