#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
struct TagComponent
{
    static int ID() { return static_cast<int>( ComponentID::Tag ); }
	static string_view Name() { return "Tag"sv; }

	static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, TagComponent& component );
    // Fields for engine/reflection: what is saved, shown and set by path.
    static void Reflect();
	static void CreateLuaBinding( sol::state& lua );

public:
	TagComponent() = default;
	TagComponent( string name, string cls = "Object"s );
	string mName;
	string mClass;
};

}
