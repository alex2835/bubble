#include "engine/pch/pch.hpp"
#include "engine/scene/components/character_controller_component.hpp"
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
CharacterControllerComponent::CharacterControllerComponent()
    : mController( CharacterController( 0.5f, 1.0f, 0.35f ) )
{
}

CharacterControllerComponent::CharacterControllerComponent( f32 radius, f32 height, f32 stepHeight )
    : mController( CharacterController( radius, height, stepHeight ) )
{
}

CharacterControllerComponent::CharacterControllerComponent( CharacterController controller )
    : mController( std::move( controller ) )
{
}

CharacterControllerComponent::~CharacterControllerComponent()
{
}

namespace
{
// Everything the inspector can set on a controller, as plain values: the
// step for a change is this, not the controller, so undo does not have to
// copy a Bullet object per frame.
struct ControllerSpec
{
    f32 mRadius, mHeight, mStepHeight, mJumpSpeed, mFallSpeed, mMaxSlopeRadians;
    vec3 mGravity;
    bool operator==( const ControllerSpec& ) const = default;

    static ControllerSpec Of( const CharacterController& c )
    {
        return { c.GetRadius(), c.GetHeight(), c.GetStepHeight(), c.GetJumpSpeed(),
                 c.GetFallSpeed(), c.GetMaxSlopeRadians(), c.GetGravity() };
    }

    void ApplyTo( CharacterControllerComponent& component ) const
    {
        auto& controller = component.mController;
        // The capsule is baked into the Bullet objects; a new size is a new
        // controller, standing where the old one stood.
        if ( controller.GetRadius() != mRadius or controller.GetHeight() != mHeight or controller.GetStepHeight() != mStepHeight )
        {
            const vec3 pos = controller.GetPosition();
            controller = CharacterController( mRadius, mHeight, mStepHeight );
            controller.Warp( pos );
        }
        controller.SetJumpSpeed( mJumpSpeed );
        controller.SetFallSpeed( mFallSpeed );
        controller.SetMaxSlope( mMaxSlopeRadians );
        controller.SetGravity( mGravity );
    }
};
}

void CharacterControllerComponent::OnComponentDraw( EditContext& ctx, const Entity& entity, CharacterControllerComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "CharacterController component" );

    const auto apply = []( CharacterControllerComponent& c, const ControllerSpec& spec ) { spec.ApplyTo( c ); };
    auto edit = [&]( const char* label, auto&& widget )
    {
        EditProperty<CharacterControllerComponent>( ctx, entity, label, ControllerSpec::Of( component.mController ), widget, apply );
    };

    // Capsule shape controls
    edit( "Radius", []( ControllerSpec& s ) { return ImGui::DragFloat( "Radius", &s.mRadius, 0.01f, 0.1f, 10.0f ); } );
    edit( "Height", []( ControllerSpec& s ) { return ImGui::DragFloat( "Height", &s.mHeight, 0.01f, 0.0f, 10.0f ); } );
    ImGui::Text( "Total Height: %.2f", component.mController.GetHeight() + 2.0f * component.mController.GetRadius() );

    ImGui::Separator();

    // Movement configuration
    edit( "Step Height", []( ControllerSpec& s ) { return ImGui::DragFloat( "Step Height", &s.mStepHeight, 0.01f, 0.0f, 2.0f ); } );
    edit( "Max Slope", []( ControllerSpec& s )
    {
        f32 degrees = glm::degrees( s.mMaxSlopeRadians );
        if ( not ImGui::SliderFloat( "Max Slope (deg)", &degrees, 0.0f, 90.0f ) )
            return false;
        s.mMaxSlopeRadians = glm::radians( degrees );
        return true;
    } );

    ImGui::Separator();

    // Jump/Fall configuration
    edit( "Jump Speed", []( ControllerSpec& s ) { return ImGui::DragFloat( "Jump Speed", &s.mJumpSpeed, 0.1f, 0.0f, 50.0f ); } );
    edit( "Fall Speed", []( ControllerSpec& s ) { return ImGui::DragFloat( "Fall Speed", &s.mFallSpeed, 0.1f, 0.0f, 100.0f ); } );
    edit( "Gravity", []( ControllerSpec& s ) { return ImGui::DragFloat3( "Gravity", &s.mGravity.x, 0.1f ); } );

    ImGui::Separator();
}

void CharacterControllerComponent::ToJson( json& j, const Project& project, const CharacterControllerComponent& component )
{
    const auto& controller = component.mController;
    j["Radius"sv] = controller.GetRadius();
    j["Height"sv] = controller.GetHeight();
    j["StepHeight"sv] = controller.GetStepHeight();
    j["JumpSpeed"sv] = controller.GetJumpSpeed();
    j["FallSpeed"sv] = controller.GetFallSpeed();
    j["MaxSlopeRadians"sv] = controller.GetMaxSlopeRadians();
    j["Gravity"sv] = controller.GetGravity();
}

void CharacterControllerComponent::FromJson( const json& j, Project& project, CharacterControllerComponent& component )
{
    f32 radius = j["Radius"sv];
    f32 height = j["Height"sv];
    f32 stepHeight = j.value( "StepHeight"sv, 0.35f );
    component.mController = CharacterController( radius, height, stepHeight );

    if ( j.contains( "JumpSpeed"sv ) )
        component.mController.SetJumpSpeed( j["JumpSpeed"sv] );
    if ( j.contains( "FallSpeed"sv ) )
        component.mController.SetFallSpeed( j["FallSpeed"sv] );
    if ( j.contains( "MaxSlopeRadians"sv ) )
        component.mController.SetMaxSlope( j["MaxSlopeRadians"sv] );
    if ( j.contains( "Gravity"sv ) )
        component.mController.SetGravity( j["Gravity"sv] );
}

void CharacterControllerComponent::CreateLuaBinding( sol::state& lua )
{
    lua.new_usertype<CharacterController>(
        "CharacterController",
        sol::constructors<CharacterController( f32, f32, f32 )>(),

        "set_walk_velocity",           &CharacterController::SetWalkVelocity,
        "set_walk_direction",          &CharacterController::SetWalkDirection,
        "set_velocity_for_time_interval",&CharacterController::SetVelocityForTimeInterval,
        "jump",                      &CharacterController::Jump,
        "warp",                      &CharacterController::Warp,
        "is_on_ground",                &CharacterController::IsOnGround,
        "get_position",               &CharacterController::GetPosition,
        "get_linear_velocity",         &CharacterController::GetLinearVelocity,
        "set_max_jump_height",          &CharacterController::SetMaxJumpHeight,
        "set_jump_speed",              &CharacterController::SetJumpSpeed,
        "set_fall_speed",              &CharacterController::SetFallSpeed,
        "set_gravity",                &CharacterController::SetGravity,
        "set_max_slope",               &CharacterController::SetMaxSlope,
        "set_step_height",             &CharacterController::SetStepHeight,
        "get_radius",                 &CharacterController::GetRadius,
        "get_height",                 &CharacterController::GetHeight
    );

    lua.new_usertype<CharacterControllerComponent>(
        "CharacterControllerComponent",
        "controller", &CharacterControllerComponent::mController
    );

    lua["create_character_controller"] = []( f32 radius, f32 height, f32 stepHeight ) {
        return CharacterController( radius, height, stepHeight );
    };
}

}
