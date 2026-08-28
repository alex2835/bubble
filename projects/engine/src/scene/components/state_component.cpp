#include "engine/pch/pch.hpp"
#include "engine/scene/components/state_component.hpp"
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
StateComponent::StateComponent()
    : mState( CreateScope<Any>( sol::nil ) )
{
}

StateComponent::StateComponent( const Any& any )
    : mState( CreateScope<Any>( AnyDeepCopy( any ) ) )
{
}

StateComponent::~StateComponent()
{

}

StateComponent::StateComponent( const StateComponent& other )
    : mState( AnyDeepCopy( other.mState ) )
{

}

StateComponent& StateComponent::operator=( const StateComponent& other )
{
    if ( this != &other )
        mState = AnyDeepCopy( other.mState );
    return *this;
}

void StateComponent::OnComponentDraw( const Project& project, const Entity& entity, StateComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "State component" );
    *component.mState = DrawAnyValue( const_cast<Project&>( project ), "State##state"sv, *component.mState );
}

void StateComponent::ToJson( json& json, const Project& project, const StateComponent& component )
{
    // validate that var state belong to proper lua state
    if ( component.mState and component.mState->is<Table>() )
    {
        lua_State* componentLua = component.mState->as<Table>().lua_state();
        lua_State* projectLua   = project.mScriptingEngine.mLua->lua_state();
        BUBBLE_ASSERT( componentLua == projectLua, "StateComponent Lua state mismatch: component belongs to a different sol::state than the project" );
    }

    json = SaveAnyValue( *component.mState );
}

void StateComponent::FromJson( const json& json, Project& project, StateComponent& component )
{
    component.mState = CreateScope<Any>( LoadAnyValue( project.mScriptingEngine, json ) );
}

void StateComponent::CreateLuaBinding( sol::state& lua )
{
    // It is native lua type
}

}
