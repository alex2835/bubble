#include "engine/pch/pch.hpp"
#include "engine/scene/components/rigid_body_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/types/array.hpp"
#include "engine/types/string.hpp"
#include "engine/utils/geometry.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace bubble
{
RigidBodyComponent::RigidBodyComponent()
    : mRigidBody( RigidBody::CreateSphere( 0, 1 ) )
{
}

RigidBodyComponent::RigidBodyComponent( RigidBody rigidBody )
    : mRigidBody( std::move( rigidBody ) )
{
}

RigidBodyComponent::~RigidBodyComponent()
{
}

namespace
{
// The body as the inspector sees it: shape and its dimensions, mass,
// friction, kinematic. A step is a change of this; applying one rebuilds the
// Bullet body only when the shape changed.
struct BodySpec
{
    int mShapeType = SPHERE_SHAPE_PROXYTYPE;
    f32 mRadius = 1.0f;       // sphere, capsule
    f32 mHeight = 1.0f;       // capsule
    vec3 mHalfExtents = vec3( 1 ); // box
    f32 mMass = 0.0f;
    f32 mFriction = 0.5f;
    bool mKinematic = false;
    bool operator==( const BodySpec& ) const = default;

    static BodySpec Of( RigidBody& body )
    {
        BodySpec spec;
        spec.mMass = body.GetMass();
        spec.mFriction = body.GetFriction();
        spec.mKinematic = body.IsKinematic();
        auto* shape = body.getShape();
        spec.mShapeType = shape->getShapeType();
        switch ( spec.mShapeType )
        {
            case SPHERE_SHAPE_PROXYTYPE:
                spec.mRadius = static_cast<btSphereShape*>( shape )->getRadius();
                break;
            case BOX_SHAPE_PROXYTYPE:
            {
                const btVector3 he = static_cast<btBoxShape*>( shape )->getHalfExtentsWithMargin();
                spec.mHalfExtents = vec3( he.x(), he.y(), he.z() );
                break;
            }
            case CAPSULE_SHAPE_PROXYTYPE:
            {
                auto* capsule = static_cast<btCapsuleShape*>( shape );
                spec.mRadius = capsule->getRadius();
                spec.mHeight = capsule->getHalfHeight() * 2.0f;
                break;
            }
        }
        return spec;
    }

    bool SameShape( const BodySpec& o ) const
    {
        return mShapeType == o.mShapeType and mRadius == o.mRadius and mHeight == o.mHeight and mHalfExtents == o.mHalfExtents;
    }

    RigidBody Build() const
    {
        switch ( mShapeType )
        {
            case BOX_SHAPE_PROXYTYPE:     return RigidBody::CreateBox( mMass, mHalfExtents );
            case CAPSULE_SHAPE_PROXYTYPE: return RigidBody::CreateCapsule( mMass, mRadius, mHeight );
            default:                      return RigidBody::CreateSphere( mMass, mRadius );
        }
    }

    void ApplyTo( RigidBodyComponent& component ) const
    {
        auto& body = component.mRigidBody;
        if ( not SameShape( Of( body ) ) )
        {
            // Every RigidBody::Create* builds a fresh body, so anything not
            // passed to it is gone. Friction and the kinematic flag have to be
            // put back: changing a shape's radius used to silently reset
            // friction to Bullet's default.
            body = Build();
        }
        else
            body.SetMass( mMass );
        body.SetFriction( mFriction );
        body.SetKinematic( mKinematic );
    }
};

// Dimensions to start a new shape from: the model's bounds if there are any.
BodySpec DefaultShape( int shapeType, const BodySpec& current, const opt<AABB>& box )
{
    BodySpec spec = current;
    spec.mShapeType = shapeType;
    switch ( shapeType )
    {
        case SPHERE_SHAPE_PROXYTYPE:
            spec.mRadius = box ? box->getShortestEdge() / 2 : 1.0f;
            break;
        case BOX_SHAPE_PROXYTYPE:
            spec.mHalfExtents = box ? ( box->getMax() - box->getMin() ) * 0.5f : vec3( 1 );
            break;
        case CAPSULE_SHAPE_PROXYTYPE:
            spec.mRadius = 0.5f;
            spec.mHeight = 1.0f;
            if ( box )
            {
                const vec3 size = box->getMax() - box->getMin();
                spec.mRadius = glm::min( size.x, size.z ) / 2.0f;
                spec.mHeight = glm::max( 0.0f, size.y - 2.0f * spec.mRadius );
            }
            break;
    }
    return spec;
}
}

void RigidBodyComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, RigidBodyComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "RigidBody component" );

    static const map<int, string_view> shapes{ { SPHERE_SHAPE_PROXYTYPE, "sphere"sv },
                                               { BOX_SHAPE_PROXYTYPE, "box"sv },
                                               { CAPSULE_SHAPE_PROXYTYPE, "capsule"sv } };

    const auto apply = []( RigidBodyComponent& c, const BodySpec& spec ) { spec.ApplyTo( c ); };
    auto edit = [&]( const char* label, auto&& widget )
    {
        return EditProperty<RigidBodyComponent>( ctx, entity, label, BodySpec::Of( component.mRigidBody ), widget, apply );
    };

    const auto box = TryGetModelBBox( ctx.mProject, entity );
    const BodySpec current = BodySpec::Of( component.mRigidBody );

    /// Physics shape selection combo
    edit( "Collision shape", [&]( BodySpec& spec )
    {
        bool changed = false;
        if ( ImGui::BeginCombo( "Collision shape", shapes.at( spec.mShapeType ).data() ) )
        {
            for ( const auto& [id, name] : shapes )
            {
                const bool selected = id == spec.mShapeType;
                if ( ImGui::Selectable( name.data(), selected ) and not selected )
                {
                    spec = DefaultShape( id, spec, box );
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    } );

    /// Rigid body controls
    edit( "Kinematic", []( BodySpec& spec )
    {
        if ( not ImGui::Checkbox( "Kinematic", &spec.mKinematic ) )
            return false;
        // Bullet only treats a massless body as kinematic, and a kinematic body
        // is driven by set_transform from a script rather than by forces - so
        // mass stops meaning anything the moment this is ticked.
        if ( spec.mKinematic )
            spec.mMass = 0.0f;
        return true;
    } );
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "Driven by set_transform from a script instead of by forces, and pushes dynamic bodies it meets. Mass must be 0." );

    ImGui::BeginDisabled( current.mKinematic );
    edit( "Mass", []( BodySpec& spec ) { return ImGui::DragFloat( "Mass", &spec.mMass ); } );
    ImGui::EndDisabled();

    edit( "Friction", []( BodySpec& spec ) { return ImGui::DragFloat( "Friction", &spec.mFriction ); } );

    /// Shape controls
    switch ( current.mShapeType )
    {
        case SPHERE_SHAPE_PROXYTYPE:
            edit( "Radius", []( BodySpec& spec ) { return ImGui::DragFloat( "Radius", &spec.mRadius ); } );
            break;
        case BOX_SHAPE_PROXYTYPE:
            edit( "Half Extends", []( BodySpec& spec ) { return ImGui::DragFloat3( "Half Extends", &spec.mHalfExtents.x ); } );
            break;
        case CAPSULE_SHAPE_PROXYTYPE:
            edit( "Radius", []( BodySpec& spec ) { return ImGui::DragFloat( "Radius", &spec.mRadius, 0.01f, 0.01f, 100.0f ); } );
            edit( "Height", []( BodySpec& spec ) { return ImGui::DragFloat( "Height", &spec.mHeight, 0.01f, 0.0f, 100.0f ); } );
            break;
    }
}

void RigidBodyComponent::ToJson( json& j, const Project& project, const RigidBodyComponent& component )
{
    const auto& rigidBody = component.mRigidBody;
    auto body = rigidBody.getBody();
    j["Mass"sv] = body->getMass();
    j["Friction"sv] = body->getFriction();
    j["Kinematic"sv] = rigidBody.IsKinematic();

    auto shape = rigidBody.getShape();
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<const btSphereShape*>( shape );
            j["Shape"sv] = "Sphere"sv;
            j["Radius"sv] = sphereShape->getRadius();
            break;
        }
        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<const btBoxShape*>( shape );
            btVector3 halfExtends = boxShape->getHalfExtentsWithMargin();
            j["Shape"sv] = "Box"sv;
            j["HalfExtends"sv] = vec3( halfExtends.x(), halfExtends.y(), halfExtends.z() );
            break;
        }
        case CAPSULE_SHAPE_PROXYTYPE:
        {
            auto capsuleShape = static_cast<const btCapsuleShape*>( shape );
            j["Shape"sv] = "Capsule"sv;
            j["Radius"sv] = capsuleShape->getRadius();
            j["Height"sv] = capsuleShape->getHalfHeight() * 2.0f;
            break;
        }
        default:
            throw std::runtime_error( "Invalid shape type" );
    }
}

void RigidBodyComponent::FromJson( const json& j, Project& project, RigidBodyComponent& component )
{
    f32 mass = j["Mass"sv];
    f32 friction = j["Friction"sv];

    string shape = j.value( "Shape"sv, j.value( "Type"sv, "Sphere"s ) ); // Support old format

    if ( shape == "Sphere"s )
        component.mRigidBody = RigidBody::CreateSphere( mass, j["Radius"sv] );
    else if ( shape == "Box"s )
        component.mRigidBody = RigidBody::CreateBox( mass, j["HalfExtends"sv] );
    else if ( shape == "Capsule"s )
        component.mRigidBody = RigidBody::CreateCapsule( mass, j["Radius"sv], j["Height"sv] );
    else
        throw std::runtime_error( "Undefined physics shape" );

    component.mRigidBody.SetFriction( friction );
    // Absent from projects saved before kinematic bodies existed.
    if ( j.value( "Kinematic"sv, false ) )
        component.mRigidBody.SetKinematic( true );
}

void RigidBodyComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<RigidBody>(
        "RigidBody",
        "get_mass",             &RigidBody::GetMass,
        "set_friction",         &RigidBody::SetFriction,
        "get_friction",         &RigidBody::GetFriction,
        "apply_central_impulse", &RigidBody::ApplyCentralImpulse,
        "apply_torque_impulse",  &RigidBody::ApplyTorqueImpulse,
        "set_kinematic",         &RigidBody::SetKinematic,
        "is_kinematic",          &RigidBody::IsKinematic,
        "set_transform",         &RigidBody::SetTransform,
        // GetTransform fills two out-params, which has no sensible Lua shape.
        // Returned as a pair instead: local pos, rot = body:get_transform()
        "get_transform",         []( const RigidBody& body )
        {
            vec3 position, rotation;
            body.GetTransform( position, rotation );
            return std::make_tuple( position, rotation );
        }
    );

    lua.new_usertype<RigidBodyComponent>(
        "RigidBodyComponent",
        "rigid_body", &RigidBodyComponent::mRigidBody
    );

    lua["create_rigid_body_sphere"] = []( const TransformComponent& trans, f32 mass, f32 radius ) {
        auto rigidBody = RigidBody::CreateSphere( mass, radius );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
    lua["create_rigid_body_box"] = []( const TransformComponent& trans, f32 mass, vec3 halfExtend ) {
        auto rigidBody = RigidBody::CreateBox( mass, halfExtend );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
    lua["create_rigid_body_capsule"] = []( const TransformComponent& trans, f32 mass, f32 radius, f32 height ) {
        auto rigidBody = RigidBody::CreateCapsule( mass, radius, height );
        rigidBody.SetTransform( trans.mPosition, trans.mRotation );
        return rigidBody;
    };
}

}
