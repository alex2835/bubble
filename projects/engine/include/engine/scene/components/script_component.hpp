#pragma once
#include "engine/scene/components/component_base.hpp"
#include <sol/function.hpp>

namespace bubble
{
struct Script;

struct ScriptComponent
{
    static int ID() { return static_cast<int>( ComponentID::Script ); }
	static string_view Name() { return "Script"sv; }

    static void OnComponentDraw( const Project& project, const Entity& entity, ScriptComponent& component );
	static void ToJson( json& json, const Project& project, const ScriptComponent& component );
	static void FromJson( const json& json, Project& project, ScriptComponent& component );
    static void CreateLuaBinding( sol::state& lua );

public:
    ScriptComponent() = default;
    ScriptComponent( const Ref<Script>& scirpt );
    ~ScriptComponent();
    Ref<Script> mScript;
    // Empty when the script defines no on_start, that callback is optional.
    sol::protected_function mOnStart;
    sol::protected_function mOnUpdate;
};

}
