#pragma once
#include "engine/scene/components/component_base.hpp"
#include <sol/sol.hpp>

namespace bubble
{
struct TagComponent
{
    static int ID() { return static_cast<int>( ComponentID::Tag ); }
	static string_view Name() { return "tag"sv; }

	static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, TagComponent& component );
    // Fields for engine/reflection: what is saved, shown and set by path.
    static void Reflect();
    // Lua: the fields come from Reflect(); these are what is added to them.
    using LuaConstructors = sol::constructors<TagComponent(), TagComponent( string ), TagComponent( string, string )>;
    static void BindLuaMethods( sol::state& lua, sol::usertype<TagComponent>& type );

public:
	TagComponent() = default;
	TagComponent( string name, string cls = "Object"s );
	string mName;
	string mClass;
};

}
