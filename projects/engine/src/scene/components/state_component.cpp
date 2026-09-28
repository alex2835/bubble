#include "engine/pch/pch.hpp"
#include "engine/scripting/reflection_lua.hpp"
#include "engine/scene/components/state_component.hpp"
#include "engine/scene/components/component_draw_utils.hpp"
#include "engine/project/project.hpp"
#include "engine/utils/imgui_utils.hpp"
#include "engine/serialization/types_serialization.hpp"
#include "engine/serialization/any_serialization.hpp"
#include "engine/editing/ui/lua_table_widget.hpp"
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

void StateComponent::Reflect()
{
    TypeBuilder<StateComponent>( Name().data() );
    const entt::meta_type type = entt::resolve<StateComponent>();
    RegisterDynamicKeys( type, DynamicKeys{
        .mGet = []( const entt::meta_any& c, const PathKey& key ) { return LuaTableGet( *c.cast<const StateComponent&>().mState, key ); },
        .mSet = []( entt::meta_any& c, const PathKey& key, const entt::meta_any& value )
        { LuaTableSet( *c.cast<const StateComponent&>().mState, key, value ); },
        .mValueType = entt::resolve<Any>(),
    } );
    // The whole table, for editor.get( e, "state" ).
    RegisterJsonCodec(
        type,
        []( const entt::meta_any& c, const ReflectionContext& ) -> json { return SaveAnyValue( *c.cast<const StateComponent&>().mState ); },
        []( const json& j, const ReflectionContext& ctx ) -> entt::meta_any
        {
            if ( not ctx.mScripting )
                throw std::runtime_error( "a state table cannot be read without a project" );
            return StateComponent( LoadAnyValue( *ctx.mScripting, j ) );
        } );
}

void StateComponent::OnComponentDraw( InspectorContext& ctx, const Entity& entity, StateComponent& component )
{
    ImGui::TextColored( TEXT_COLOR, "State component" );
    DrawLuaTable( ctx, StateTableRoot( ctx.mScene, entity ) );
}

LuaTableRoot StateComponent::StateTableRoot( Scene& scene, Entity entity )
{
    return LuaTableRoot{ &scene, entity, "State", []( Scene& s, Entity e ) -> opt<Table>
    {
        if ( not s.HasComponent<StateComponent>( e ) )
            return std::nullopt;
        const auto& state = s.GetComponent<StateComponent>( e ).mState;
        if ( not state or not state->is<Table>() )
            return std::nullopt;
        return state->as<Table>();
    } };
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
