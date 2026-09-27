#pragma once
#include "engine/scene/components/component_base.hpp"
#include "engine/types/array.hpp"
#include <recs/entity.hpp>

namespace bubble
{
// Where an entity hangs in the scene's tree: its parent, and its children in
// the order the Entities window lists them. Every entity has one; only the
// scene's root has no parent.
//
// Both sides of every link are kept - the parent to go up, the children to
// go down and to keep the order - and both are written by the functions in
// engine/scene/hierarchy.hpp and nowhere else, which is what keeps them in
// step. A file stores both.
//
// With a parent, the entity's TransformComponent is relative to the parent's;
// its place in the world is the parent's world transform times its own.
struct HierarchyComponent
{
    static int ID() { return static_cast<int>( ComponentID::Hierarchy ); }
    static string_view Name() { return "Hierarchy"sv; }

    static void OnComponentDraw( InspectorContext& ctx, const Entity& entity, HierarchyComponent& component );
    static void ToJson( json& json, const Project& project, const HierarchyComponent& component );
    static void FromJson( const json& json, Project& project, HierarchyComponent& component );
    static void CreateLuaBinding( sol::state& lua );

    Entity mParent = INVALID_ENTITY;
    vector<Entity> mChildren;
};

}
