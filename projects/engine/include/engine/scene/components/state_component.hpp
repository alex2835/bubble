#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
struct StateComponent
{
    static int ID() { return static_cast<int>( ComponentID::State ); }
    static string_view Name() { return "state"sv; }

    // For engine/reflection: the table's keys are the component's -
    // "state.health", "state.items[1]" - for property.set and editor.get.
    // The inspector, the file and Lua stay written by hand.
    static void Reflect();

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, StateComponent& component );
    // How an edit addresses this entity's state table - see lua_value_command.hpp.
    static LuaTableRoot StateTableRoot( Scene& scene, Entity entity );
    static void ToJson( json& json, const Project& project, const StateComponent& component );
    static void FromJson( const json& json, Project& project, StateComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    StateComponent();
    ~StateComponent();
    StateComponent( const Any& any );
    StateComponent( const StateComponent& );
    StateComponent& operator=( const StateComponent& );
    // A copy deep copies the table; a move - which is also how the pool
    // relocates the component - hands the same table over, so what scripts
    // hold of it stays this entity's.
    StateComponent( StateComponent&& ) noexcept = default;
    StateComponent& operator=( StateComponent&& ) noexcept = default;
    Scope<Any> mState;
};

}
