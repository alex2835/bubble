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
// Every RigidBody::Create* builds a fresh body, so anything not passed to it is
// gone. Friction and the kinematic flag have to be put back: changing a shape's
// radius used to silently reset friction to Bullet's default.
void ReplaceBody( RigidBodyComponent& component, RigidBody body, f32 friction, bool kinematic )
{
    component.mRigidBody = std::move( body );
    component.mRigidBody.SetFriction( friction );
    component.mRigidBody.SetKinematic( kinematic );
}
}

void RigidBodyComponent::OnComponentDraw( const Project& project, const Entity& entity, RigidBodyComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "RigidBody component" );

    static map<int, string_view> shapes{ { SPHERE_SHAPE_PROXYTYPE, "sphere"sv },
                                         { BOX_SHAPE_PROXYTYPE, "box"sv },
                                         { CAPSULE_SHAPE_PROXYTYPE, "capsule"sv } };

    auto& rigidBody = component.mRigidBody;
    auto shape = rigidBody.getShape();
    auto box = TryGetEntityBBox( project, entity );

    f32 mass = rigidBody.GetMass();
    f32 friction = rigidBody.GetFriction();
    bool kinematic = rigidBody.IsKinematic();

    /// Physics shape selection combo
    string_view curShapeName = shapes[shape->getShapeType()];
    if ( ImGui::BeginCombo( "Collision shape", curShapeName.data() ) )
    {
        for ( const auto& [id, name] : shapes )
        {
            bool isSelected = curShapeName == name;
            if ( ImGui::Selectable( name.data(), isSelected ) )
            {
                if ( id == SPHERE_SHAPE_PROXYTYPE )
                {
                    f32 radius = 1.0f;
                    if ( box )
                        radius = box->getShortestEdge() / 2;
                    ReplaceBody( component, RigidBody::CreateSphere( mass, radius ), friction, kinematic );
                }
                if ( id == BOX_SHAPE_PROXYTYPE )
                {
                    vec3 halfExtend( 1 );
                    if ( box )
                        halfExtend = ( box->getMax() - box->getMin() ) * 0.5f;
                    ReplaceBody( component, RigidBody::CreateBox( mass, halfExtend ), friction, kinematic );
                }
                if ( id == CAPSULE_SHAPE_PROXYTYPE )
                {
                    f32 radius = 0.5f;
                    f32 height = 1.0f;
                    if ( box )
                    {
                        vec3 size = box->getMax() - box->getMin();
                        radius = glm::min( size.x, size.z ) / 2.0f;
                        height = glm::max( 0.0f, size.y - 2.0f * radius );
                    }
                    ReplaceBody( component, RigidBody::CreateCapsule( mass, radius, height ), friction, kinematic );
                }
            }
        }
        ImGui::EndCombo();
        return;
    }

    /// Rigid body controls
    if ( ImGui::Checkbox( "Kinematic", &kinematic ) )
    {
        // Bullet only treats a massless body as kinematic, and a kinematic body
        // is driven by set_transform from a script rather than by forces - so
        // mass stops meaning anything the moment this is ticked.
        if ( kinematic )
        {
            mass = 0.0f;
            rigidBody.SetMass( 0.0f );
        }
        rigidBody.SetKinematic( kinematic );
    }
    if ( ImGui::IsItemHovered() )
        ImGui::SetTooltip( "Driven by set_transform from a script instead of by forces, and pushes dynamic bodies it meets. Mass must be 0." );

    ImGui::BeginDisabled( kinematic );
    if ( ImGui::DragFloat( "Mass", &mass ) )
        rigidBody.SetMass( mass );
    ImGui::EndDisabled();

    if ( ImGui::DragFloat( "Friction", &friction ) )
        rigidBody.SetFriction( friction );

    /// Shape controls
    switch ( shape->getShapeType() )
    {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            auto sphereShape = static_cast<btSphereShape*>( shape );
            f32 radius = sphereShape->getRadius();
            if ( ImGui::DragFloat( "Radius", &radius ) )
                ReplaceBody( component, RigidBody::CreateSphere( mass, radius ), friction, kinematic );
        } break;

        case BOX_SHAPE_PROXYTYPE:
        {
            auto boxShape = static_cast<btBoxShape*>( shape );
            btVector3 he = boxShape->getHalfExtentsWithMargin();
            auto halfExtends = vec3( he.x(), he.y(), he.z() );
            if ( ImGui::DragFloat3( "Half Extends", &halfExtends.x ) )
                ReplaceBody( component, RigidBody::CreateBox( mass, halfExtends ), friction, kinematic );
        } break;

        case CAPSULE_SHAPE_PROXYTYPE:
        {
            auto capsuleShape = static_cast<btCapsuleShape*>( shape );
            f32 radius = capsuleShape->getRadius();
            f32 height = capsuleShape->getHalfHeight() * 2.0f;
            bool changed = false;
            changed |= ImGui::DragFloat( "Radius", &radius, 0.01f, 0.01f, 100.0f );
            changed |= ImGui::DragFloat( "Height", &height, 0.01f, 0.0f, 100.0f );
            if ( changed )
                ReplaceBody( component, RigidBody::CreateCapsule( mass, radius, height ), friction, kinematic );
        } break;
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
