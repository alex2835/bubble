#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/any.hpp"

namespace bubble
{
struct StateComponent
{
    static int ID() { return static_cast<int>( ComponentID::State ); }
    static string_view Name() { return "State"sv; }

    static void OnComponentDraw( EditContext& ctx, const Entity& entity, StateComponent& component );
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
    Scope<Any> mState;
};

}
