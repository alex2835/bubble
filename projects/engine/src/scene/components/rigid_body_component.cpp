#include "engine/pch/pch.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/transform_component.hpp"
#include "engine/reflection/reflection.hpp"
#include "engine/types/glm.hpp"
#include <sol/sol.hpp>

namespace bubble
{
namespace
{
RigidBody Build( const RigidBodyComponent& c )
{
    switch ( c.mShape )
    {
        case BodyShape::Box:     return RigidBody::CreateBox( c.mMass, c.mHalfExtents );
        case BodyShape::Capsule: return RigidBody::CreateCapsule( c.mMass, c.mRadius, c.mHeight );
        default:                 return RigidBody::CreateSphere( c.mMass, c.mRadius );
    }
}

// Whether the Bullet body is still the one the fields describe: its mass and
// its shape. Friction and the kinematic flag are set on it in place.
bool BodyMatches( const RigidBodyComponent& c )
{
    // GetMass is not const.
    auto& body = const_cast<RigidBody&>( c.mRigidBody );
    if ( body.GetMass() != c.mMass )
        return false;
    const btCollisionShape* shape = body.getShape();
    switch ( c.mShape )
    {
        case BodyShape::Sphere:
            return shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE and
                   static_cast<const btSphereShape*>( shape )->getRadius() == c.mRadius;
        case BodyShape::Box:
        {
            if ( shape->getShapeType() != BOX_SHAPE_PROXYTYPE )
                return false;
            const btVector3 he = static_cast<const btBoxShape*>( shape )->getHalfExtentsWithMargin();
            return vec3( he.x(), he.y(), he.z() ) == c.mHalfExtents;
        }
        case BodyShape::Capsule:
        {
            if ( shape->getShapeType() != CAPSULE_SHAPE_PROXYTYPE )
                return false;
            const auto* capsule = static_cast<const btCapsuleShape*>( shape );
            return capsule->getRadius() == c.mRadius and capsule->getHalfHeight() * 2.0f == c.mHeight;
        }
    }
    return false;
}

void Changed( RigidBodyComponent& c )
{
    c.Apply();
}

bool HasRadius( const entt::meta_any& c ) { return c.cast<const RigidBodyComponent&>().mShape != BodyShape::Box; }
bool IsBox( const entt::meta_any& c ) { return c.cast<const RigidBodyComponent&>().mShape == BodyShape::Box; }
bool IsCapsule( const entt::meta_any& c ) { return c.cast<const RigidBodyComponent&>().mShape == BodyShape::Capsule; }
bool NotKinematic( const entt::meta_any& c ) { return not c.cast<const RigidBodyComponent&>().mKinematic; }
}

void RigidBodyComponent::Reflect()
{
    ReflectEnum<BodyShape>();
    TypeBuilder<RigidBodyComponent>( Name().data() )
        .Field<&RigidBodyComponent::mShape>( "shape" )
        .Field<&RigidBodyComponent::mRadius>( "radius", { .mMin = 0.01f, .mSpeed = 0.01f, .mVisible = HasRadius } )
        .Field<&RigidBodyComponent::mHeight>( "height", { .mMin = 0.0f, .mSpeed = 0.01f, .mVisible = IsCapsule } )
        .Field<&RigidBodyComponent::mHalfExtents>( "half_extents", { .mMin = 0.01f, .mSpeed = 0.01f, .mVisible = IsBox } )
        .Field<&RigidBodyComponent::mMass>( "mass", { .mMin = 0.0f, .mSpeed = 0.1f, .mVisible = NotKinematic } )
        .Field<&RigidBodyComponent::mFriction>( "friction", { .mMin = 0.0f, .mSpeed = 0.01f } )
        .Field<&RigidBodyComponent::mKinematic>( "kinematic", {
            .mTooltip = "Driven by set_transform from a script instead of by forces, and pushes dynamic bodies it meets. Its mass is 0." } )
        .OnChanged<&Changed>();
}

void RigidBodyComponent::BindLuaMethods( sol::state&, sol::usertype<RigidBodyComponent>& type )
{
    type["apply_central_impulse"] = []( RigidBodyComponent& c, const vec3& impulse ) { c.mRigidBody.ApplyCentralImpulse( impulse ); };
    type["apply_torque_impulse"] = []( RigidBodyComponent& c, const vec3& impulse ) { c.mRigidBody.ApplyTorqueImpulse( impulse ); };
    // Rotations cross to Lua as Euler radians, like transform's.
    type["set_transform"] = []( RigidBodyComponent& c, const vec3& position, const vec3& rotation )
    {
        c.mRigidBody.SetTransform( position, Transform::FromEuler( rotation ) );
    };
    // local position, rotation = body:get_transform()
    type["get_transform"] = []( const RigidBodyComponent& c )
    {
        vec3 position;
        quat rotation;
        c.mRigidBody.GetTransform( position, rotation );
        return std::make_tuple( position, Transform::ToEuler( rotation ) );
    };
}

RigidBodyComponent::RigidBodyComponent()
    : mRigidBody( RigidBody::CreateSphere( 0.0f, 1.0f ) )
{
    Rebuild();
}

RigidBodyComponent::~RigidBodyComponent()
{
}

void RigidBodyComponent::Apply()
{
    if ( mKinematic )
        mMass = 0.0f;
    if ( not BodyMatches( *this ) )
    {
        if ( mRigidBody.getBody()->isInWorld() )
            mRebuild = true;
        else
            Rebuild();
        return;
    }
    mRigidBody.SetFriction( mFriction );
    mRigidBody.SetKinematic( mKinematic );
}

void RigidBodyComponent::Rebuild()
{
    // Every RigidBody::Create* builds a fresh body, so what is not passed to
    // it is set again after.
    mRigidBody = Build( *this );
    mRigidBody.SetFriction( mFriction );
    mRigidBody.SetKinematic( mKinematic );
    mRebuild = false;
}

}
