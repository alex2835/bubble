#pragma once
#include "engine/scene/components/component_base.hpp"
#include <sol/sol.hpp>
#include <sol/function.hpp>

namespace bubble
{
struct Script;

struct ScriptComponent
{
    static int ID() { return static_cast<int>( ComponentID::Script ); }
	static string_view Name() { return "script"sv; }

    // Fields for engine/reflection: the script, a resource saved by its path.
    static void Reflect();

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
