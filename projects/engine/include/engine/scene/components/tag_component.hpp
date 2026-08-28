#pragma once
#include "engine/scene/components/component_base.hpp"

namespace bubble
{
struct TagComponent
{
    static int ID() { return static_cast<int>( ComponentID::Tag ); }
	static string_view Name() { return "Tag"sv; }

	static void OnComponentDraw( const Project& project, const Entity& entity, TagComponent& component );
	static void ToJson( json& json, const Project& project, const TagComponent& component );
	static void FromJson( const json& json, Project& project, TagComponent& component );
	static void CreateLuaBinding( sol::state& lua );

public:
	TagComponent() = default;
	TagComponent( string name, string cls = "Object"s );
	string mName;
	string mClass;
};

}
